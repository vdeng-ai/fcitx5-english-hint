#include "translation_worker.h"

#include <fcitx-utils/event.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
using namespace fcitx;
using namespace fcitx::english_hint;
using namespace std::chrono_literals;

void require(bool value, const char *message) {
    if (!value) { throw std::runtime_error(message); }
}

// A slow loopback-only OpenAI fixture. No real endpoint or API key is loaded.
class MockServer {
public:
    explicit MockServer(int count) : count_(count) {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        require(fd_ >= 0, "mock socket must open");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) ||
            listen(fd_, 4)) {
            close(fd_);
            throw std::runtime_error("mock server must bind loopback");
        }
        socklen_t size = sizeof(address);
        getsockname(fd_, reinterpret_cast<sockaddr *>(&address), &size);
        endpoint = "http://127.0.0.1:" + std::to_string(ntohs(address.sin_port));
        thread_ = std::thread([this] { run(); });
    }
    ~MockServer() {
        shutdown(fd_, SHUT_RDWR);
        thread_.join();
        close(fd_);
    }
    std::vector<std::string> requests() {
        std::lock_guard<std::mutex> lock(mutex_);
        return requests_;
    }
    std::string endpoint;
private:
    void run() {
        while (true) {
            const int client = accept(fd_, nullptr, nullptr);
            if (client < 0) { return; }
            timeval timeout{2, 0};
            setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            std::string request;
            char buffer[4096];
            while (true) {
                const auto bytes = recv(client, buffer, sizeof(buffer), 0);
                if (bytes <= 0) { break; }
                request.append(buffer, bytes);
                const auto headerEnd = request.find("\r\n\r\n");
                const auto lengthPos = request.find("Content-Length:");
                if (headerEnd != std::string::npos && lengthPos != std::string::npos &&
                    request.size() >= headerEnd + 4 + std::stoul(request.substr(lengthPos + 15))) {
                    break;
                }
            }
            {
                std::lock_guard<std::mutex> lock(mutex_);
                requests_.push_back(request);
            }
            std::this_thread::sleep_for(150ms);
            std::string content;
            for (int i = 0; i < count_; ++i) {
                if (i) { content += "\\n"; }
                content += std::to_string(i) + "\\tEnglish " + std::to_string(i);
            }
            const std::string body = "{\"choices\":[{\"message\":{\"content\":\"" +
                content + "\"},\"finish_reason\":\"stop\"}]}";
            const std::string response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " +
                std::to_string(body.size()) + "\r\n\r\n" + body;
            std::size_t sent = 0;
            while (sent < response.size()) {
                const auto bytes = send(client, response.data() + sent,
                                        response.size() - sent, MSG_NOSIGNAL);
                if (bytes <= 0) { break; }
                sent += bytes;
            }
            close(client);
        }
    }
    int fd_;
    int count_;
    std::thread thread_;
    std::mutex mutex_;
    std::vector<std::string> requests_;
};

void checkBatch(int count, const std::filesystem::path &path) {
    MockServer server(count);
    EnglishHintConfig config;
    config.endpoint = server.endpoint;
    config.maxBatch = count;
    config.debounceMs = 50;
    config.timeoutMs = 2000;
    TranslationCache cache(64);
    PersistentCache persisted(path.string());
    EventLoop loop;
    EventDispatcher dispatcher;
    dispatcher.attach(&loop);
    const auto mainThread = std::this_thread::get_id();
    int refreshes = 0;
    int ticks = 0;
    bool callbackOnMain = true;
    bool submittedNewPage = false;
    bool timedOut = false;
    std::vector<std::string> oldPage, newPage;
    for (int i = 0; i < count; ++i) {
        oldPage.push_back("旧候选" + std::to_string(i));
        newPage.push_back("新候选" + std::to_string(i));
    }
    const auto start = std::chrono::steady_clock::now();
    {
        TranslationWorker worker(config, cache, &persisted, dispatcher, [&] {
            callbackOnMain &= std::this_thread::get_id() == mainThread;
            ++refreshes;
            loop.exit();
        });
        require(worker.submit({"被替换候选"}), "first page must enqueue");
        auto oversized = oldPage;
        oversized.push_back("超出上限候选");
        require(worker.submit(oversized), "latest page must replace pending page");
        require(!worker.submit(oldPage), "duplicate pending page must be suppressed");
        require(std::chrono::steady_clock::now() - start < 100ms,
                "submitting a larger batch must not wait for the server");
        auto timer = loop.addTimeEvent(CLOCK_MONOTONIC, now(CLOCK_MONOTONIC), 1000,
            [&](EventSourceTime *source, uint64_t) {
                ++ticks;
                if (!submittedNewPage && !server.requests().empty()) {
                    require(!worker.submit(oldPage), "duplicate in-flight page must be suppressed");
                    require(worker.submit(newPage), "new page must enqueue while old request is in flight");
                    submittedNewPage = true;
                }
                if (std::chrono::steady_clock::now() - start > 5s) {
                    timedOut = true;
                    loop.exit();
                    return false;
                }
                source->setTime(now(CLOCK_MONOTONIC) + 10000);
                return true;
            });
        timer->setEnabled(true);
        loop.exec();
        require(!timedOut && refreshes == 1 && callbackOnMain,
                "only the current async page must refresh on the main loop");
        require(ticks >= 10, "main loop must remain responsive during slow requests");
    }
    dispatcher.detach();
    const auto requests = server.requests();
    require(requests.size() == 2, "debounce and duplicate suppression must limit requests to two");
    for (int page = 0; page < 2; ++page) {
        require(requests[page].find("被替换候选") == std::string::npos &&
                    requests[page].find("超出上限候选") == std::string::npos,
                "superseded and out-of-range candidates must never be sent");
        for (int i = 0; i < count; ++i) {
            const auto &word = page ? newPage[i] : oldPage[i];
            require(requests[page].find(word) != std::string::npos,
                    "every candidate in the configured batch must be sent");
            std::string translation;
            require(cache.get(word, translation) && translation == "English " + std::to_string(i),
                    "both stale and current results must remain reusable in memory");
        }
    }
    require(persisted.load().size() == static_cast<std::size_t>(2 * count),
            "all translations must survive persistent-cache reload");
    std::cout << "batch=" << count << " requests=2 refreshes=" << refreshes
              << " main-loop-ticks=" << ticks << '\n';
}
} // namespace

int main() {
    char pattern[] = "/tmp/english-hint-worker-XXXXXX";
    const auto directory = mkdtemp(pattern);
    if (!directory) { return 1; }
    const std::filesystem::path root(directory);
    int result = 0;
    try {
        for (int count : {5, 8, 10}) {
            checkBatch(count, root / std::to_string(count));
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    std::filesystem::remove_all(root);
    return result;
}
