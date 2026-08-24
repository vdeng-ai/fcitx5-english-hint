#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace fcitx::english_hint {

std::string formatAuxTranslations(
    const std::vector<std::pair<std::string, std::string>> &entries);

} // namespace fcitx::english_hint
