#include "english_commit.h"

#include <fcitx/candidatelist.h>

namespace fcitx::english_hint {

std::string selectedEnglishTranslation(const InputPanel &panel,
                                      const DisplayedTranslations &displayed,
                                      int index) {
    if (displayed.auxLine.empty() ||
        panel.auxDown().toString() != displayed.auxLine) {
        return {};
    }

    const auto list = panel.candidateList();
    if (!list || list->empty()) {
        return {};
    }

    if (index < 0 || index >= list->size() ||
        static_cast<std::size_t>(index) >= displayed.candidates.size()) {
        return {};
    }

    const auto &candidate = list->candidate(index);
    const auto &[source, translation] = displayed.candidates[index];
    if (candidate.isPlaceHolder() || source.empty() || translation.empty() ||
        candidate.text().toString() != source) {
        return {};
    }
    return translation;
}

} // namespace fcitx::english_hint
