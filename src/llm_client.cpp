#include "llm_client.h"

#include <curl/curl.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fcitx::english_hint {
namespace {

size_t writeResponse(char *ptr, size_t size, size_t nmemb, void *userdata) {
    const size_t bytes = size * nmemb;
    static_cast<std::string *>(userdata)->append(ptr, bytes);
    return bytes;
}

std::string jsonEscape(std::string_view input) {
    std::string out;
    out.reserve(input.size() + 16);
    for (const unsigned char ch : input) {
        switch (ch) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (ch < 0x20) {
                constexpr char hex[] = "0123456789abcdef";
                out += "\\u00";
                out += hex[(ch >> 4) & 0x0f];
                out += hex[ch & 0x0f];
            } else {
                out.push_back(static_cast<char>(ch));
            }
        }
    }
    return out;
}

void appendUtf8(std::string &out, uint32_t codepoint) {
    if (codepoint <= 0x7f) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        out.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0x10ffff) {
        out.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

std::optional<std::string> parseJsonString(const std::string &json,
                                           size_t quotePos) {
    if (quotePos >= json.size() || json[quotePos] != '"') {
        return std::nullopt;
    }

    std::string out;
    for (size_t i = quotePos + 1; i < json.size(); ++i) {
        const char ch = json[i];
        if (ch == '"') {
            return out;
        }
        if (ch != '\\') {
            out.push_back(ch);
            continue;
        }
        if (++i >= json.size()) {
            return std::nullopt;
        }
        switch (json[i]) {
        case '"':
            out.push_back('"');
            break;
        case '\\':
            out.push_back('\\');
            break;
        case '/':
            out.push_back('/');
            break;
        case 'b':
            out.push_back('\b');
            break;
        case 'f':
            out.push_back('\f');
            break;
        case 'n':
            out.push_back('\n');
            break;
        case 'r':
            out.push_back('\r');
            break;
        case 't':
            out.push_back('\t');
            break;
        case 'u': {
            if (i + 4 >= json.size()) {
                return std::nullopt;
            }
            uint32_t codepoint = 0;
            for (int j = 0; j < 4; ++j) {
                const char hex = json[++i];
                codepoint <<= 4;
                if (hex >= '0' && hex <= '9') {
                    codepoint |= static_cast<uint32_t>(hex - '0');
                } else if (hex >= 'a' && hex <= 'f') {
                    codepoint |= static_cast<uint32_t>(hex - 'a' + 10);
                } else if (hex >= 'A' && hex <= 'F') {
                    codepoint |= static_cast<uint32_t>(hex - 'A' + 10);
                } else {
                    return std::nullopt;
                }
            }
            appendUtf8(out, codepoint);
            break;
        }
        default:
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::optional<std::string> extractJsonStringField(const std::string &json,
                                                  std::string_view field,
                                                  size_t start = 0) {
    const std::string needle = "\"" + std::string(field) + "\"";
    size_t pos = json.find(needle, start);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    ++pos;
    while (pos < json.size() &&
           std::isspace(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    return parseJsonString(json, pos);
}

std::optional<std::string> extractAssistantContent(const std::string &json) {
    size_t pos = json.find("\"message\"");
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    return extractJsonStringField(json, "content", pos);
}

bool completionWasTruncated(const std::string &json) {
    const auto reason = extractJsonStringField(json, "finish_reason");
    return reason && *reason == "length";
}

std::string trim(std::string value) {
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(),
                value.end());
    return value;
}

bool isLocalEndpoint(std::string_view endpoint) {
    const auto scheme = endpoint.find("://");
    size_t hostStart = scheme == std::string_view::npos ? 0 : scheme + 3;
    if (hostStart >= endpoint.size()) {
        return false;
    }

    size_t hostEnd = std::string_view::npos;
    if (endpoint[hostStart] == '[') {
        const auto closeBracket = endpoint.find(']', hostStart + 1);
        if (closeBracket == std::string_view::npos) {
            return false;
        }
        hostEnd = closeBracket + 1;
    } else {
        hostEnd = endpoint.find_first_of(":/?", hostStart);
        if (hostEnd == std::string_view::npos) {
            hostEnd = endpoint.size();
        }
    }
    const auto host = endpoint.substr(hostStart, hostEnd - hostStart);

    if (host == "localhost" || host == "::1" || host == "[::1]" ||
        host.rfind("127.", 0) == 0 || host.rfind("10.", 0) == 0 ||
        host.rfind("192.168.", 0) == 0) {
        return true;
    }

    if (host.rfind("172.", 0) == 0) {
        const auto secondDot = host.find('.', 4);
        if (secondDot != std::string_view::npos) {
            int secondOctet = 0;
            const auto parsed = std::from_chars(host.data() + 4,
                                                host.data() + secondDot,
                                                secondOctet);
            return parsed.ec == std::errc{} && secondOctet >= 16 &&
                   secondOctet <= 31;
        }
    }
    return false;
}

std::vector<std::string> parseTranslations(const std::string &content,
                                           size_t expected) {
    std::vector<std::string> result(expected);
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto tab = line.find('\t');
        if (tab == std::string::npos) {
            continue;
        }

        size_t index = 0;
        const auto *begin = line.data();
        const auto *end = line.data() + tab;
        const auto parsed = std::from_chars(begin, end, index);
        if (parsed.ec != std::errc{} || parsed.ptr != end || index >= expected) {
            continue;
        }

        std::string translation = trim(line.substr(tab + 1));
        if (translation.size() > 160) {
            translation.resize(160);
        }
        result[index] = std::move(translation);
    }
    return result;
}

} // namespace

LlmClient::LlmClient(EnglishHintConfig config) : config_(std::move(config)) {
    static std::once_flag curlInit;
    std::call_once(curlInit, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    curl_ = curl_easy_init();

    headers_ = curl_slist_append(headers_, "Content-Type: application/json");
    if (!config_.apiKey.empty()) {
        const std::string auth = "Authorization: Bearer " + config_.apiKey;
        headers_ = curl_slist_append(headers_, auth.c_str());
    }
}

LlmClient::~LlmClient() {
    if (headers_) {
        curl_slist_free_all(headers_);
    }
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
}

std::vector<std::string>
LlmClient::translate(const std::vector<std::string> &candidates) {
    std::vector<std::string> empty(candidates.size());
    if (!curl_ || candidates.empty()) {
        return empty;
    }

    std::ostringstream userPrompt;
    for (size_t i = 0; i < candidates.size(); ++i) {
        userPrompt << i << '\t' << candidates[i];
        if (i + 1 != candidates.size()) {
            userPrompt << '\n';
        }
    }

    constexpr std::string_view systemPrompt =
        "Translate Chinese IME text to concise natural English. Return only "
        "INDEX<TAB>ENGLISH lines, one per input.";

    const std::string escapedPrompt = jsonEscape(userPrompt.str());
    const bool localEndpoint = isLocalEndpoint(config_.endpoint);

    auto perform = [&](bool disableReasoning, std::string &response,
                       long &httpCode) {
        std::string payload =
            "{\"model\":\"" + jsonEscape(config_.model) +
            "\",\"messages\":[{\"role\":\"system\",\"content\":\"" +
            jsonEscape(systemPrompt) +
            "\"},{\"role\":\"user\",\"content\":\"" + escapedPrompt +
            "\"}],\"temperature\":0,\"max_tokens\":" +
            std::to_string(config_.maxTokens);
        if (disableReasoning) {
            payload += ",\"reasoning_effort\":\"none\"";
        }
        payload += ",\"stream\":false}";

        response.clear();
        response.reserve(1024);
        httpCode = 0;

        curl_easy_reset(curl_);
        curl_easy_setopt(curl_, CURLOPT_URL, config_.endpoint.c_str());
        curl_easy_setopt(curl_, CURLOPT_POST, 1L);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE,
                         static_cast<long>(payload.size()));
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeResponse);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT_MS, 700L);
        curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS,
                         static_cast<long>(config_.timeoutMs));
        curl_easy_setopt(curl_, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl_, CURLOPT_TCP_NODELAY, 1L);
        curl_easy_setopt(curl_, CURLOPT_FRESH_CONNECT, 0L);
        curl_easy_setopt(curl_, CURLOPT_FORBID_REUSE, 0L);
        if (localEndpoint) {
            // Local/private model endpoints should never be routed through a
            // desktop HTTP(S) proxy; this also avoids proxy-induced latency.
            curl_easy_setopt(curl_, CURLOPT_NOPROXY, "*");
        }

        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers_);

        const CURLcode status = curl_easy_perform(curl_);
        if (status == CURLE_OK) {
            curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &httpCode);
        }
        return status;
    };

    std::string response;
    long httpCode = 0;
    CURLcode status = perform(true, response, httpCode);

    // Some OpenAI-compatible servers reject reasoning_effort. Retry once
    // without the optional field instead of making users add a compatibility
    // switch to the configuration UI.
    if (status == CURLE_OK && httpCode == 400) {
        status = perform(false, response, httpCode);
    }

    if (status != CURLE_OK || httpCode < 200 || httpCode >= 300) {
        return empty;
    }

    const auto content = extractAssistantContent(response);
    if (!content) {
        return empty;
    }

    auto translations = parseTranslations(*content, candidates.size());
    if (completionWasTruncated(response)) {
        // The final line is the one most likely to be cut mid-sentence. Never
        // cache it as a valid translation; earlier complete lines remain useful
        // and the missing candidate will be retried by the worker/UI pipeline.
        for (auto it = translations.rbegin(); it != translations.rend(); ++it) {
            if (!it->empty()) {
                it->clear();
                break;
            }
        }
    }
    return translations;
}

} // namespace fcitx::english_hint
