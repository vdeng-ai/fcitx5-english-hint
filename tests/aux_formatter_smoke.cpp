#include "aux_formatter.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

int main() {
    using fcitx::english_hint::formatAuxTranslations;

    const std::vector<std::pair<std::string, std::string>> entries = {
        {"1", "Improve efficiency"},
        {"2.", "Improve benefits"},
        {"3", "Boost efficiency"},
        {"4", "Speed up"},
        {"5", "Improve performance"},
    };

    const std::string expected =
        "1.Improve efficiency  2.Improve benefits  3.Boost efficiency  "
        "4.Speed up  5.Improve performance";
    const std::string actual = formatAuxTranslations(entries);
    if (actual != expected) {
        std::cerr << "unexpected auxDown line:\n" << actual << '\n';
        return 1;
    }

    const std::vector<std::pair<std::string, std::string>> partialEntries = {
        {"1", "Improve efficiency"},
        {"3", "Boost efficiency"},
        {"5", "Improve performance"},
    };
    const std::string partialExpected =
        "1.Improve efficiency  3.Boost efficiency  5.Improve performance";
    const std::string partialActual = formatAuxTranslations(partialEntries);
    if (partialActual != partialExpected) {
        std::cerr << "unexpected partial auxDown line:\n"
                  << partialActual << '\n';
        return 2;
    }

    std::cout << actual << '\n' << partialActual << '\n';
    return 0;
}
