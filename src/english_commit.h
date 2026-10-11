#pragma once

#include <fcitx/inputpanel.h>

#include <string>
#include <utility>
#include <vector>

namespace fcitx::english_hint {

struct DisplayedTranslations {
    std::string auxLine;
    // Indexed by the current page's candidate position; missing entries stay empty.
    std::vector<std::pair<std::string, std::string>> candidates;
    // Vertical mode owns a candidate-list display instead of an aux line.
    std::weak_ptr<CandidateList> inlineList;
};

// Only select a translation that is still displayed for the same candidate.
std::string selectedEnglishTranslation(const InputPanel &panel,
                                      const DisplayedTranslations &displayed,
                                      int index);

} // namespace fcitx::english_hint
