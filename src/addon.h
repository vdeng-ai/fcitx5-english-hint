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
    bool isRimeCandidateText(InputContext *inputContext,
                             const Text &text) const;
    bool containsHan(const std::string &text) const;
    bool shouldTranslate(const std::string &text) const;
    std::vector<std::string>
    collectMissingCandidates(InputContext *inputContext);
    void filterOutput(InputContext *inputContext, Text &text);
    void refreshCurrentInputPanel();

    Instance *instance_;
    EnglishHintFcitxConfig uiConfig_;
    EnglishHintConfig config_;
    TranslationCache cache_;
    PersistentCache persistentCache_;
    EventDispatcher dispatcher_;
    std::unique_ptr<TranslationWorker> worker_;
    ScopedConnection outputFilterConnection_;
};

} // namespace fcitx::english_hint
