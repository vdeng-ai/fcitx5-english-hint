#include "addon.h"
#include "local_dictionary.h"

#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx/inputpanel.h>
#include <fcitx/userinterface.h>

#include <string>
#include <utility>

namespace fcitx::english_hint {

FCITX_DEFINE_LOG_CATEGORY(english_hint_log, "english-hint");

EnglishHint::EnglishHint(Instance *instance)
    : instance_(instance), cache_(config_.cacheSize),
      persistentCache_(persistentCachePath()) {
    dispatcher_.attach(&instance_->eventLoop());
    loadEnglishHintConfig(uiConfig_);
    config_ = toRuntimeConfig(uiConfig_);
    cache_.setCapacity(config_.cacheSize);
    const auto persisted = persistentCache_.load();
    for (const auto &[source, translation] : persisted) {
        cache_.put(source, translation);
    }
    applyRuntimeConfig();

    outputFilterConnection_ = instance_->connect<Instance::OutputFilter>(
        [this](InputContext *inputContext, Text &text) {
            filterOutput(inputContext, text);
        });
}

EnglishHint::~EnglishHint() {
    worker_.reset();
    dispatcher_.detach();
}

void EnglishHint::applyRuntimeConfig() {
    worker_.reset();
    config_ = toRuntimeConfig(uiConfig_);
    cache_.setCapacity(config_.cacheSize);

    if (config_.enabled) {
        worker_ = std::make_unique<TranslationWorker>(
            config_, cache_, &persistentCache_, dispatcher_,
            [this] { refreshCurrentInputPanel(); });
    }

    FCITX_INFO() << "fcitx5-english-hint configured: enabled="
                 << config_.enabled << ", endpoint=" << config_.endpoint
                 << ", model=" << config_.model
                 << ", max_tokens=" << config_.maxTokens
                 << ", debounce=" << config_.debounceMs << "ms";
}

void EnglishHint::reloadConfig() {
    loadEnglishHintConfig(uiConfig_);
    applyRuntimeConfig();
}

const Configuration *EnglishHint::getConfig() const { return &uiConfig_; }

void EnglishHint::setConfig(const RawConfig &config) {
    uiConfig_.load(config, true);
    if (!saveEnglishHintConfig(uiConfig_)) {
        FCITX_WARN() << "Failed to save english-hint configuration";
    }
    applyRuntimeConfig();
}

bool EnglishHint::isRimeCandidateText(InputContext *inputContext,
                                      const Text &text) const {
    if (!inputContext || text.empty()) {
        return false;
    }

    if (inputContext->capabilityFlags().testAny(
            CapabilityFlag::PasswordOrSensitive)) {
        return false;
    }

    const auto *entry = instance_->inputMethodEntry(inputContext);
    if (!entry || entry->addon() != "rime") {
        return false;
    }

    const auto &panel = inputContext->inputPanel();
    const auto candidateList = panel.candidateList();
    if (!candidateList || candidateList->empty()) {
        return false;
    }

    const std::string rendered = text.toString();
    for (int i = 0; i < candidateList->size(); ++i) {
        const auto &candidate = candidateList->candidate(i);
        if (candidate.isPlaceHolder()) {
            continue;
        }
        if (candidate.text().toString() == rendered) {
            return true;
        }
    }

    return false;
}

bool EnglishHint::containsHan(const std::string &text) const {
    auto it = text.begin();
    while (it != text.end()) {
        uint32_t codepoint = 0;
        const auto next = utf8::getNextChar(it, text.end(), &codepoint);
        if (!utf8::isValidChar(codepoint)) {
            return false;
        }

        if ((codepoint >= 0x3400 && codepoint <= 0x4dbf) ||
            (codepoint >= 0x4e00 && codepoint <= 0x9fff) ||
            (codepoint >= 0xf900 && codepoint <= 0xfaff) ||
            (codepoint >= 0x20000 && codepoint <= 0x2ebef)) {
            return true;
        }
        it = next;
    }
    return false;
}

bool EnglishHint::shouldTranslate(const std::string &text) const {
    if (text.empty() || !containsHan(text)) {
        return false;
    }

    const auto length = utf8::lengthValidated(text);
    if (length == utf8::INVALID_LENGTH || length > 32) {
        return false;
    }

    return text.find('\n') == std::string::npos &&
           text.find('\r') == std::string::npos;
}

std::vector<std::string>
EnglishHint::collectMissingCandidates(InputContext *inputContext) {
    std::vector<std::string> missing;
    if (!inputContext) {
        return missing;
    }

    const auto candidateList = inputContext->inputPanel().candidateList();
    if (!candidateList) {
        return missing;
    }

    missing.reserve(config_.maxBatch);
    for (int i = 0; i < candidateList->size() &&
                    missing.size() < config_.maxBatch;
         ++i) {
        const auto &candidate = candidateList->candidate(i);
        if (candidate.isPlaceHolder()) {
            continue;
        }

        const std::string value = candidate.text().toString();
        if (!shouldTranslate(value)) {
            continue;
        }

        std::string cached;
        if (cache_.get(value, cached)) {
            continue;
        }

        if (lookupLocalDictionary(value, cached)) {
            cache_.put(value, cached);
            continue;
        }
        missing.push_back(value);
    }
    return missing;
}

void EnglishHint::filterOutput(InputContext *inputContext, Text &text) {
    if (!config_.enabled || !worker_ ||
        !isRimeCandidateText(inputContext, text)) {
        return;
    }

    const std::string candidate = text.toString();
    if (!shouldTranslate(candidate)) {
        return;
    }

    std::string translation;
    if (cache_.get(candidate, translation)) {
        text.append(" [" + translation + "]");
        return;
    }

    if (lookupLocalDictionary(candidate, translation)) {
        cache_.put(candidate, translation);
        text.append(" [" + translation + "]");
        worker_->submit(collectMissingCandidates(inputContext));
        return;
    }

    worker_->submit(collectMissingCandidates(inputContext));
}

void EnglishHint::refreshCurrentInputPanel() {
    auto *inputContext = instance_->mostRecentInputContext();
    if (!inputContext) {
        return;
    }

    const auto *entry = instance_->inputMethodEntry(inputContext);
    if (!entry || entry->addon() != "rime") {
        return;
    }

    inputContext->updateUserInterface(UserInterfaceComponent::InputPanel, true);
}

class EnglishHintFactory final : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override {
        return new EnglishHint(manager->instance());
    }
};

} // namespace fcitx::english_hint

FCITX_ADDON_FACTORY(fcitx::english_hint::EnglishHintFactory)
