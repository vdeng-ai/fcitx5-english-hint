#include "config.h"
#include "llm_client.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

const std::vector<std::string> kLongSource = {
    "这个方案可以继续推进下去",
    "我觉得这个问题还需要进一步讨论",
    "明天上午我们再确认一下具体安排",
    "目前这个功能基本已经可以正常使用了",
    "如果没有其他问题就按照这个方案执行",
};

bool printAndCheck(const std::vector<std::string> &translated,
                   bool requireAll) {
    if (translated.size() != kLongSource.size()) {
        std::cerr << "translation size mismatch\n";
        return false;
    }

    bool allPresent = true;
    for (std::size_t i = 0; i < kLongSource.size(); ++i) {
        std::cout << kLongSource[i] << " => "
                  << (translated[i].empty() ? "<missing>" : translated[i])
                  << '\n';
        allPresent = allPresent && !translated[i].empty();
    }
    return requireAll ? allPresent : true;
}

} // namespace

int main() {
    auto config = fcitx::english_hint::loadEnglishHintConfig();

    // Regression: a deliberately small budget may truncate the final item.
    // The client must never treat a cut-off last line as a valid translation.
    auto constrained = config;
    constrained.maxTokens = 48;
    fcitx::english_hint::LlmClient constrainedClient(constrained);
    const auto constrainedResult = constrainedClient.translate(kLongSource);
    std::cout << "--- constrained max_tokens=48 ---\n";
    if (!printAndCheck(constrainedResult, false)) {
        return 1;
    }
    if (!constrainedResult.empty() && !constrainedResult.back().empty()) {
        std::cerr << "expected truncated final item to be discarded\n";
        return 2;
    }

    // Normal v0.8 default: five medium-length IME phrases should complete.
    config.maxTokens = 96;
    fcitx::english_hint::LlmClient normalClient(config);
    const auto normalResult = normalClient.translate(kLongSource);
    std::cout << "--- normal max_tokens=96 ---\n";
    if (!printAndCheck(normalResult, true)) {
        return 3;
    }
    return 0;
}
