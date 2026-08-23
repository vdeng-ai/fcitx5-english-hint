#include "config.h"
#include "llm_client.h"

#include <iostream>
#include <string>
#include <vector>

int main() {
    auto config = fcitx::english_hint::loadEnglishHintConfig();
    fcitx::english_hint::LlmClient client(config);

    const std::vector<std::string> source = {
        "提高效率", "今天天气不错", "这事就这么定了",
        "我们明天再讨论", "这个方案可以继续推进"};
    const auto translated = client.translate(source);

    if (translated.size() != source.size()) {
        std::cerr << "translation size mismatch\n";
        return 1;
    }

    bool ok = true;
    for (std::size_t i = 0; i < source.size(); ++i) {
        std::cout << source[i] << " => " << translated[i] << '\n';
        ok = ok && !translated[i].empty();
    }
    return ok ? 0 : 2;
}
