#pragma once

#include <fcitx-config/configuration.h>
#include <fcitx-config/option.h>

#include <cstddef>
#include <string>

namespace fcitx::english_hint {

struct EnglishHintConfig {
    bool enabled = true;
    std::string endpoint =
        "http://127.0.0.1:8080/v1/chat/completions";
    std::string model = "qwen3.8-27b";
    std::string apiKey;
    int debounceMs = 200;
    int timeoutMs = 2500;
    int maxTokens = 96;
    std::size_t cacheSize = 4096;
    std::size_t maxBatch = 5;
    bool debug = false;
};

FCITX_CONFIGURATION(
    EnglishHintGeneralConfig,
    Option<bool> enabled{this, "Enabled", "启用英文提示", true};
    Option<int, IntConstrain> debounceMs{
        this, "DebounceMs", "防抖时间（毫秒）", 200, IntConstrain(0, 2000)};
    Option<int, IntConstrain> timeoutMs{
        this, "TimeoutMs", "请求超时（毫秒）", 2500,
        IntConstrain(250, 15000)};
    Option<int, IntConstrain> cacheSize{
        this, "CacheSize", "内存缓存条目数", 4096,
        IntConstrain(32, 65536)};
    Option<int, IntConstrain> maxBatch{
        this, "MaxBatch", "每次请求最多候选数", 5,
        IntConstrain(1, 5)};
    Option<bool> debug{this, "Debug", "输出调试性能指标", false};
)

FCITX_CONFIGURATION(
    EnglishHintLlmConfig,
    Option<std::string> endpoint{
        this, "Endpoint", "OpenAI-compatible Chat Completions 地址",
        "http://127.0.0.1:8080/v1/chat/completions"};
    Option<std::string> model{this, "Model", "模型名称", "qwen3.8-27b"};
    Option<std::string> apiKey{this, "ApiKey", "API Key（可留空）", ""};
    Option<int, IntConstrain> maxTokens{
        this, "MaxTokens", "最大输出 Token 数", 96,
        IntConstrain(8, 256)};
)

FCITX_CONFIGURATION(
    EnglishHintFcitxConfig,
    Option<EnglishHintGeneralConfig> general{
        this, "General", "通用", EnglishHintGeneralConfig{}};
    Option<EnglishHintLlmConfig> llm{
        this, "LLM", "LLM", EnglishHintLlmConfig{}};
)

inline constexpr const char *kEnglishHintConfigFile =
    "conf/english-hint.conf";

EnglishHintConfig toRuntimeConfig(const EnglishHintFcitxConfig &config);
void loadEnglishHintConfig(EnglishHintFcitxConfig &config);
bool saveEnglishHintConfig(const EnglishHintFcitxConfig &config);
EnglishHintConfig loadEnglishHintConfig();

} // namespace fcitx::english_hint
