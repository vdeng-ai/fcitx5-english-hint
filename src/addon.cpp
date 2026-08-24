#include "addon.h"
#include "aux_formatter.h"
#include "local_dictionary.h"

#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/utf8.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx/inputpanel.h>
#include <fcitx/userinterface.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

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

    updateUIWatcher_ = instance_->watchEvent(
        EventType::InputContextUpdateUI, EventWatcherPhase::PostInputMethod,
        [this](Event &event) {
            auto &uiEvent = static_cast<InputContextUpdateUIEvent &>(event);
            if (uiEvent.component() != UserInterfaceComponent::InputPanel) {
                return;
            }
            handleInputPanelUpdate(uiEvent.inputContext());
        });

    destroyedWatcher_ = instance_->watchEvent(
        EventType::InputContextDestroyed, EventWatcherPhase::PostInputMethod,
        [this](Event &event) {
            auto &contextEvent = static_cast<InputContextEvent &>(event);
            ownedAuxDown_.erase(contextEvent.inputContext());
        });
}

EnglishHint::~EnglishHint() {
    destroyedWatcher_.reset();
    updateUIWatcher_.reset();
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
    refreshCurrentInputPanel();
}

const Configuration *EnglishHint::getConfig() const { return &uiConfig_; }

void EnglishHint::setConfig(const RawConfig &config) {
    uiConfig_.load(config, true);
    if (!saveEnglishHintConfig(uiConfig_)) {
        FCITX_WARN() << "Failed to save english-hint configuration";
    }
    applyRuntimeConfig();
    refreshCurrentInputPanel();
}

bool EnglishHint::isEligibleContext(InputContext *inputContext) const {
    if (!config_.enabled || !worker_ || !inputContext) {
        return false;
    }

    if (inputContext->capabilityFlags().testAny(
            CapabilityFlag::PasswordOrSensitive)) {
        return false;
    }

    const auto *entry = instance_->inputMethodEntry(inputContext);
    return entry && entry->addon() == "rime";
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

void EnglishHint::clearOwnedAuxDown(InputContext *inputContext) {
    if (!inputContext) {
        return;
    }

    const auto owned = ownedAuxDown_.find(inputContext);
    if (owned == ownedAuxDown_.end()) {
        return;
    }

    auto &panel = inputContext->inputPanel();
    if (panel.auxDown().toString() == owned->second) {
        panel.setAuxDown(Text{});
    }
    ownedAuxDown_.erase(owned);
}

void EnglishHint::handleInputPanelUpdate(InputContext *inputContext) {
    if (!inputContext) {
        return;
    }

    auto &panel = inputContext->inputPanel();
    const auto owned = ownedAuxDown_.find(inputContext);
    const std::string currentAux = panel.auxDown().toString();

    // Never overwrite auxDown content owned by Rime or another addon. We only
    // take ownership when it is empty or still contains our previous line.
    const bool ownsCurrent =
        owned != ownedAuxDown_.end() && currentAux == owned->second;
    if (!currentAux.empty() && !ownsCurrent) {
        ownedAuxDown_.erase(inputContext);
        return;
    }

    if (!isEligibleContext(inputContext)) {
        clearOwnedAuxDown(inputContext);
        return;
    }

    const auto candidateList = panel.candidateList();
    if (!candidateList || candidateList->empty()) {
        clearOwnedAuxDown(inputContext);
        return;
    }

    const std::size_t visibleCount = std::min<std::size_t>(
        config_.maxBatch, static_cast<std::size_t>(candidateList->size()));
    std::vector<std::string> missing;
    std::vector<std::pair<std::string, std::string>> translated;
    missing.reserve(visibleCount);
    translated.reserve(visibleCount);

    for (std::size_t i = 0; i < visibleCount; ++i) {
        const auto &candidate = candidateList->candidate(static_cast<int>(i));
        if (candidate.isPlaceHolder()) {
            continue;
        }

        const std::string source = candidate.text().toString();
        if (!shouldTranslate(source)) {
            continue;
        }

        std::string translation;
        if (!cache_.get(source, translation)) {
            if (lookupLocalDictionary(source, translation)) {
                cache_.put(source, translation);
            } else {
                missing.push_back(source);
                continue;
            }
        }

        std::string label =
            candidateList->label(static_cast<int>(i)).toString();
        if (label.empty()) {
            label = std::to_string(i + 1);
        }
        translated.emplace_back(std::move(label), std::move(translation));
    }

    const std::string auxLine = formatAuxTranslations(translated);
    if (auxLine.empty()) {
        clearOwnedAuxDown(inputContext);
    } else {
        if (currentAux != auxLine) {
            panel.setAuxDown(Text(auxLine));
        }
        ownedAuxDown_[inputContext] = auxLine;
    }

    // Show any cached/dictionary translations immediately, even when some
    // candidates still need the LLM. Missing entries keep their original
    // candidate labels, so the partial aux line remains unambiguous.
    if (!missing.empty()) {
        worker_->submit(std::move(missing));
    }
}

void EnglishHint::refreshCurrentInputPanel() {
    auto *inputContext = instance_->mostRecentInputContext();
    if (!inputContext) {
        return;
    }

    // Worker completion arrives on the Fcitx main loop through EventDispatcher.
    // Rebuild auxDown explicitly before asking the UI to repaint. Relying only
    // on an InputContextUpdateUI watcher is insufficient on some UI paths: the
    // repaint may happen without our watcher rebuilding the panel first, which
    // makes fresh translations appear only on the next key event.
    handleInputPanelUpdate(inputContext);
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
