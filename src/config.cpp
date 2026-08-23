#include "config.h"

#include <fcitx-config/iniparser.h>

namespace fcitx::english_hint {

EnglishHintConfig toRuntimeConfig(const EnglishHintFcitxConfig &config) {
    EnglishHintConfig runtime;
    const auto &general = *config.general;
    const auto &llm = *config.llm;

    runtime.enabled = *general.enabled;
    runtime.debounceMs = *general.debounceMs;
    runtime.timeoutMs = *general.timeoutMs;
    runtime.cacheSize = static_cast<std::size_t>(*general.cacheSize);
    runtime.maxBatch = static_cast<std::size_t>(*general.maxBatch);
    runtime.debug = *general.debug;

    runtime.endpoint = *llm.endpoint;
    runtime.model = *llm.model;
    runtime.apiKey = *llm.apiKey;
    runtime.maxTokens = *llm.maxTokens;
    return runtime;
}

void loadEnglishHintConfig(EnglishHintFcitxConfig &config) {
    readAsIni(config, kEnglishHintConfigFile);
}

bool saveEnglishHintConfig(const EnglishHintFcitxConfig &config) {
    return safeSaveAsIni(config, kEnglishHintConfigFile);
}

EnglishHintConfig loadEnglishHintConfig() {
    EnglishHintFcitxConfig config;
    loadEnglishHintConfig(config);
    return toRuntimeConfig(config);
}

} // namespace fcitx::english_hint
