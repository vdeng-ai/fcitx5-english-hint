#pragma once

#include "config.h"
#include "llm_client.h"
#include "lru_cache.h"
#include "persistent_cache.h"

#include <fcitx-utils/eventdispatcher.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fcitx::english_hint {

class TranslationWorker {
public:
    TranslationWorker(EnglishHintConfig config, TranslationCache &cache,
                      PersistentCache *persistentCache,
                      EventDispatcher &dispatcher,
                      std::function<void()> refreshCallback);
    ~TranslationWorker();

    TranslationWorker(const TranslationWorker &) = delete;
    TranslationWorker &operator=(const TranslationWorker &) = delete;

    // Thread-safe. Keeps only the latest waiting candidate snapshot. Returns
    // false when the same snapshot is already pending or in-flight.
    bool submit(std::vector<std::string> candidates);

private:
    static std::string signature(const std::vector<std::string> &candidates);
    void run();

    EnglishHintConfig config_;
    TranslationCache &cache_;
    PersistentCache *persistentCache_;
    EventDispatcher &dispatcher_;
    std::function<void()> refreshCallback_;
    LlmClient client_;

    std::mutex mutex_;
    std::condition_variable condition_;
    bool stopping_ = false;
    bool hasPending_ = false;
    std::vector<std::string> pending_;
    std::string pendingSignature_;
    std::string inFlightSignature_;
    std::uint64_t pendingGeneration_ = 0;
    std::atomic<std::uint64_t> latestGeneration_{0};
    std::chrono::steady_clock::time_point lastSubmit_;
    std::thread thread_;

    std::uint64_t requestCount_ = 0;
    std::uint64_t failureCount_ = 0;
    std::uint64_t staleCount_ = 0;
};

} // namespace fcitx::english_hint
