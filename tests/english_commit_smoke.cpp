#include "config.h"
#include "english_commit.h"

#include <fcitx-config/iniparser.h>
#include <fcitx-utils/key.h>
#include <fcitx/candidatelist.h>

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class TestCandidateList : public fcitx::CommonCandidateList {
public:
    int cursorIndex() const override { return cursor; }
    int cursor = 0;
};

class Placeholder : public fcitx::DisplayOnlyCandidateWord {
public:
    Placeholder() : DisplayOnlyCandidateWord(fcitx::Text("占位")) {
        setPlaceHolder(true);
    }
};

} // namespace

int main() {
    using namespace fcitx;
    using namespace fcitx::english_hint;

    try {
        EnglishHintFcitxConfig config;
        require(*config.general->commitEnglishWithAlt,
                "Alt+number English selection must be enabled by default");
        for (int digit = 1; digit <= 9; ++digit) {
            require(Key("Alt+" + std::to_string(digit))
                        .digitSelection(KeyState::Alt) == digit - 1,
                    "Alt+number must map to that candidate's page position");
        }
        require(Key("Alt+0").digitSelection(KeyState::Alt) == 9,
                "Alt+0 must map to the tenth position, never the first");
        require(Key("Alt+KP_2").digitSelection(KeyState::Alt) == 1,
                "keypad digits must use the same mapping");
        for (const auto *key : {"1", "2", "Return", "Control+Alt+Return",
                                "Control+2", "Control+Alt+2", "Alt+Shift+2",
                                "Alt+Shift+KP_2", "Super+Alt+2"}) {
            require(Key(key).digitSelection(KeyState::Alt) == -1,
                    "ordinary selection, old shortcut and extra modifiers must not match");
        }

        // Disabling English selection must survive configuration round-trip.
        {
            auto general = config.general.mutableValue();
            general->commitEnglishWithAlt.setValue(false);
        }
        RawConfig raw;
        config.save(raw);
        EnglishHintFcitxConfig restored;
        restored.load(raw, true);
        require(!*restored.general->commitEnglishWithAlt,
                "disabled English selection must stay disabled");
        char disabledIni[] = "[General]\nCommitEnglishWithAlt=False\n";
        auto *file = fmemopen(disabledIni, sizeof(disabledIni) - 1, "r");
        require(file != nullptr, "in-memory INI stream must open");
        RawConfig disabledRaw;
        readFromIni(disabledRaw, file);
        fclose(file);
        EnglishHintFcitxConfig disabled;
        disabled.load(disabledRaw);
        require(!*disabled.general->commitEnglishWithAlt,
                "INI flag must disable Alt+number English selection");

        InputPanel panel(nullptr);
        auto candidates = std::make_unique<TestCandidateList>();
        auto *list = candidates.get();
        list->setPageSize(5);
        for (const auto *source : {"你好", "谢谢", "未知词", "再见"}) {
            list->append<DisplayOnlyCandidateWord>(Text(source));
        }
        list->append<Placeholder>();
        list->append<DisplayOnlyCandidateWord>(Text("下一页"));
        panel.setCandidateList(std::move(candidates));
        const DisplayedTranslations displayed{
            "1.Hello  2.Thank you  4.Goodbye",
            {{"你好", "Hello"}, {"谢谢", "Thank you"}, {},
             {"再见", "Goodbye"}, {"占位", "dummy"}}, {}};
        panel.setAuxDown(Text(displayed.auxLine));
        for (int cursor : {0, 1, 3, -1}) {
            list->cursor = cursor;
            require(selectedEnglishTranslation(
                        panel, displayed, Key("Alt+1").digitSelection(KeyState::Alt)) == "Hello",
                    "Alt+1 must always select first candidate regardless of highlight");
            require(selectedEnglishTranslation(
                        panel, displayed, Key("Alt+2").digitSelection(KeyState::Alt)) == "Thank you",
                    "Alt+2 must always select second candidate regardless of highlight");
            require(selectedEnglishTranslation(
                        panel, displayed, Key("Alt+4").digitSelection(KeyState::Alt)) == "Goodbye",
                    "partial translations must keep original candidate numbers");
        }
        for (int index : {2, 4, 5, 9, -1, -2}) {
            require(selectedEnglishTranslation(panel, displayed, index).empty(),
                    "missing, placeholder and invalid positions must be ignored");
        }
        panel.setAuxDown(Text("Rime-owned content"));
        require(selectedEnglishTranslation(panel, displayed, 0).empty(),
                "another addon's aux line must not permit English commit");
        panel.setAuxDown(Text(displayed.auxLine));
        list->next();
        require(selectedEnglishTranslation(panel, displayed, 0).empty(),
                "stale translation must not be used after paging");
        panel.setCandidateList(nullptr);
        require(selectedEnglishTranslation(panel, displayed, 0).empty(),
                "cleared composition must not permit English commit");
        require(selectedEnglishTranslation(panel, {}, 0).empty(),
                "empty display snapshot must not permit English commit");
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "English selection and shortcut configuration checks passed\n";
    return 0;
}
