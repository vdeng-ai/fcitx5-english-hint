#pragma once

#include <string>

namespace fcitx::english_hint {

bool lookupLocalDictionary(const std::string &text, std::string &translation);

} // namespace fcitx::english_hint
