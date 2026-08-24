#pragma once

#include "config.h"
#include "lru_cache.h"
#include "persistent_cache.h"
#include "translation_worker.h"

#include <fcitx-utils/connectableobject.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/signals.h>
#include <fcitx/addoninstance.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fcitx::english_hint {

class EnglishHint final : public AddonInstance {
public:
    explicit EnglishHint(Instance *instance);
    ~EnglishHint() override;

    void reloadConfig() override;
    const Configuration *getConfig() const override;
    void setConfig(const RawConfig &config) override;

private:
    void applyRuntimeConfig();
    bool isEligibleContext(InputContext *inputContext) const;
    bool containsHan(const std::string &text) const;
    bool shouldTranslate(const std::string &text) const;
    void handleInputPanelUpdate(InputContext *inputContext);
    void clearOwnedAuxDown(InputContext *inputContext);
    void refreshCurrentInputPanel();

    Instance *instance_;
    EnglishHintFcitxConfig uiConfig_;
    EnglishHintConfig config_;
    TranslationCache cache_;
    PersistentCache persistentCache_;
    EventDispatcher dispatcher_;
    std::unique_ptr<TranslationWorker> worker_;
    std::unique_ptr<HandlerTableEntry<EventHandler>> updateUIWatcher_;
    std::unique_ptr<HandlerTableEntry<EventHandler>> destroyedWatcher_;
    std::unordered_map<InputContext *, std::string> ownedAuxDown_;
};

} // namespace fcitx::english_hint
