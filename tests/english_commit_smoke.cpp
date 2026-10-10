#include "config.h"
#include "english_commit.h"

#include <fcitx-config/iniparser.h>
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
        require(Key("Control+Alt+Return")
                    .checkKeyList(*config.general->commitEnglishKeys),
                "default shortcut must match");
        require(!Key("Return").checkKeyList(*config.general->commitEnglishKeys),
                "ordinary Enter must not match");
        require(!Key("Control+Return")
                     .checkKeyList(*config.general->commitEnglishKeys),
                "partial modifier combination must not match");

        // Round-trip a custom shortcut and an empty list (disabled).
        {
            auto general = config.general.mutableValue();
            general->commitEnglishKeys.setValue(KeyList{Key("Control+Shift+e")});
        }
        RawConfig raw;
        config.save(raw);
        EnglishHintFcitxConfig restored;
        restored.load(raw, true);
        require(Key("Control+Shift+e")
                    .checkKeyList(*restored.general->commitEnglishKeys),
                "custom shortcut must survive configuration round-trip");
        {
            auto general = config.general.mutableValue();
            general->commitEnglishKeys.setValue(KeyList{});
        }
        config.save(raw);
        restored.load(raw, true);
        require(restored.general->commitEnglishKeys->empty(),
                "empty shortcut list must stay disabled");
        char disabledIni[] = "[General/CommitEnglishKeys]\n";
        auto *file = fmemopen(disabledIni, sizeof(disabledIni) - 1, "r");
        require(file != nullptr, "in-memory INI stream must open");
        RawConfig disabledRaw;
        readFromIni(disabledRaw, file);
        fclose(file);
        EnglishHintFcitxConfig disabled;
        disabled.load(disabledRaw);
        require(disabled.general->commitEnglishKeys->empty(),
                "empty INI shortcut section must disable the shortcut");

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
            "1.Hello  2.Thank you",
            {{"你好", "Hello"}, {"谢谢", "Thank you"}, {}, {}, {"占位", "dummy"}}};
        panel.setAuxDown(Text(displayed.auxLine));
        require(selectedEnglishTranslation(panel, displayed) == "Hello",
                "first candidate translation must be selected");
        list->cursor = 1;
        require(selectedEnglishTranslation(panel, displayed) == "Thank you",
                "highlighted candidate translation must be selected");
        for (int index : {2, 3, 4, 5, -2}) {
            list->cursor = index;
            require(selectedEnglishTranslation(panel, displayed).empty(),
                    "missing, placeholder and invalid positions must be ignored");
        }
        list->cursor = -1;
        require(selectedEnglishTranslation(panel, displayed) == "Hello",
                "unselected cursor must use the first candidate");
        panel.setAuxDown(Text("Rime-owned content"));
        require(selectedEnglishTranslation(panel, displayed).empty(),
                "another addon's aux line must not permit English commit");
        panel.setAuxDown(Text(displayed.auxLine));
        list->next();
        require(selectedEnglishTranslation(panel, displayed).empty(),
                "stale translation must not be used after paging");
        panel.setCandidateList(nullptr);
        require(selectedEnglishTranslation(panel, displayed).empty(),
                "cleared composition must not permit English commit");
        require(selectedEnglishTranslation(panel, {}).empty(),
                "empty display snapshot must not permit English commit");
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "English selection and shortcut configuration checks passed\n";
    return 0;
}
