#pragma once

#include "config.h"

#include <curl/curl.h>

#include <string>
#include <vector>

namespace fcitx::english_hint {

class LlmClient {
public:
    explicit LlmClient(EnglishHintConfig config);
    ~LlmClient();

    LlmClient(const LlmClient &) = delete;
    LlmClient &operator=(const LlmClient &) = delete;

    std::vector<std::string>
    translate(const std::vector<std::string> &candidates);

private:
    EnglishHintConfig config_;
    CURL *curl_ = nullptr;
    curl_slist *headers_ = nullptr;
};

} // namespace fcitx::english_hint
