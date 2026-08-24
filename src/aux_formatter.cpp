#include "aux_formatter.h"

#include <algorithm>
#include <cctype>

namespace fcitx::english_hint {
namespace {

std::string normalizedLabel(std::string label, std::size_t fallbackIndex) {
    if (label.empty()) {
        label = std::to_string(fallbackIndex + 1);
    }

    const bool allDigits = !label.empty() &&
        std::all_of(label.begin(), label.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        });
    if (allDigits) {
        label.push_back('.');
    }
    return label;
}

} // namespace

std::string formatAuxTranslations(
    const std::vector<std::pair<std::string, std::string>> &entries) {
    std::string output;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto &[label, translation] = entries[i];
        if (translation.empty()) {
            continue;
        }
        if (!output.empty()) {
            output.append("  ");
        }
        output += normalizedLabel(label, i);
        output += translation;
    }
    return output;
}

} // namespace fcitx::english_hint
