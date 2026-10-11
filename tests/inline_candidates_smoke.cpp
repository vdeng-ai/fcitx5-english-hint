#include "config.h"
#include "english_commit.h"
#include "inline_candidates.h"

#include <fcitx-utils/key.h>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char *message) {
    if (!value) { throw std::runtime_error(message); }
}

// A selectable word records the original text. Simulate a real select callback
// that synchronously replaces the panel, destroying the display adapter.
class SelectableWord : public fcitx::CandidateWord {
public:
    SelectableWord(fcitx::Text text, std::string *committed,
                   fcitx::InputPanel *panel)
        : CandidateWord(std::move(text)), committed_(committed), panel_(panel) {}
    void select(fcitx::InputContext *) const override {
        auto text = this->text().toStringForCommit();
        auto *committed = committed_;
        auto *panel = panel_;
        panel->setCandidateList(nullptr);
        *committed = std::move(text);
    }
    void customLabel() { setCustomLabel(fcitx::Text("②")); }
private:
    std::string *committed_;
    fcitx::InputPanel *panel_;
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
        require(!toRuntimeConfig(config).verticalBilingual,
                "existing installations must keep aux display by default");
        config.general.mutableValue()->verticalBilingual.setValue(true);
        RawConfig raw;
        config.save(raw);
        EnglishHintFcitxConfig restored;
        restored.load(raw, true);
        require(toRuntimeConfig(restored).verticalBilingual,
                "vertical mode must survive configuration round-trip");

        InputPanel panel(nullptr);
        std::string committed;
        auto original = std::make_unique<CommonCandidateList>();
        original->setPageSize(5);
        original->setLayoutHint(CandidateLayoutHint::Horizontal);
        original->append<SelectableWord>(Text("你好"), &committed, &panel);
        auto custom = std::make_unique<SelectableWord>(Text("谢谢"), &committed, &panel);
        custom->customLabel();
        original->append(std::move(custom));
        original->append<DisplayOnlyCandidateWord>(Text("缺失翻译"));
        original->append<Placeholder>();
        original->append<DisplayOnlyCandidateWord>(Text("再见"));
        original->append<DisplayOnlyCandidateWord>(Text("下一页"));
        original->append<DisplayOnlyCandidateWord>(Text("你好"));
        panel.setCandidateList(std::move(original));
        const auto source = panel.candidateList();
        const CandidateTranslations entries{
            {"你好", "Hello"}, {"谢谢", "Thank you"}, {},
            {"占位", "never shown"}, {"再见", "Goodbye"}};
        setInlineCandidates(panel, entries);
        auto list = panel.candidateList();
        DisplayedTranslations displayed{"", entries, list};
        require(list->layoutHint() == CandidateLayoutHint::Vertical,
                "inline mode must request a vertical candidate layout");
        require(list->candidate(0).text().toString() == "你好  Hello",
                "Chinese and English must share a candidate row");
        require(list->candidate(0).text().toStringForCommit() == "你好",
                "even text-based commit must exclude the translation");
        require(source->candidate(0).text().toString() == "你好",
                "original Chinese candidate must remain unchanged");
        require(list->candidate(1).hasCustomLabel() &&
                    list->candidate(1).customLabel().toString() == "②",
                "custom labels must survive decoration");
        require(list->label(1).toString() == source->label(1).toString(),
                "selection labels must remain unchanged");
        require(list->candidate(2).text().toString() == "缺失翻译" &&
                    list->candidate(3).isPlaceHolder(),
                "missing translations and placeholders must keep their positions");
        require(selectedEnglishTranslation(panel, displayed, 1) == "Thank you" &&
                    selectedEnglishTranslation(panel, displayed, 2).empty() &&
                    selectedEnglishTranslation(panel, displayed, 3).empty(),
                "Alt selection must use displayed page positions only");
        require(list->toPageable() && list->toBulk() && list->toModifiable() &&
                    list->toCursorMovable(),
                "original optional interfaces must be preserved");
        list->toCursorMovable()->nextCandidate();
        require(list->cursorIndex() == source->cursorIndex() &&
                    selectedEnglishTranslation(panel, displayed, 0) == "Hello",
                "highlight movement must not change Alt-number selection");
        list->toPageable()->next();
        require(list->candidate(0).text().toString() == "下一页" &&
                    selectedEnglishTranslation(panel, displayed, 0).empty(),
                "paging must immediately invalidate previous translations");
        list->toPageable()->prev();
        require(selectedEnglishTranslation(panel, displayed, 0).empty(),
                "paging back must await a fresh display snapshot");
        setInlineCandidates(panel, entries);
        require(originalCandidates(panel) == source,
                "async refresh must not nest candidate adapters");
        require(list->toBulk()->candidateFromAll(5).text().toString() == "下一页",
                "global candidate indices must remain unchanged");
        source->toPageable()->next();
        require(selectedEnglishTranslation(panel, displayed, 0).empty(),
                "direct original-list paging must also reject stale translations");
        source->toPageable()->prev();
        setInlineCandidates(panel, entries);
        list->toModifiable()->replace(0,
            std::make_unique<DisplayOnlyCandidateWord>(Text("替换")));
        require(selectedEnglishTranslation(panel, displayed, 0).empty() &&
                    list->candidate(0).text().toString() == "替换",
                "candidate replacement must invalidate display translations");
        clearInlineCandidates(panel);
        require(list->layoutHint() == CandidateLayoutHint::Horizontal &&
                    &list->candidate(1) == &source->candidate(1) &&
                    selectedEnglishTranslation(panel, displayed, 1).empty(),
                "disabling inline mode must restore original layout and words");

        // Both mouse selection and digit selection use CandidateWord::select.
        for (bool numeric : {false, true}) {
            setInlineCandidates(panel, entries);
            const int index = numeric ? Key("2").digitSelection() : 1;
            require(index == 1, "ordinary digit must retain original index");
            list.reset();
            panel.candidateList()->candidate(index).select(nullptr);
            require(committed == "谢谢" && !panel.candidateList(),
                    "selection must call the original word and commit only Chinese");
            // Recreate a composition for the next selection path.
            auto fresh = std::make_unique<CommonCandidateList>();
            fresh->append<SelectableWord>(Text("你好"), &committed, &panel);
            fresh->append<SelectableWord>(Text("谢谢"), &committed, &panel);
            panel.setCandidateList(std::move(fresh));
        }
        // A new composition with identical Chinese must not inherit ownership.
        setInlineCandidates(panel, entries);
        require(selectedEnglishTranslation(panel, displayed, 1).empty(),
                "replaced candidate list must reject the old display snapshot");
        InputPanel plain(nullptr);
        auto displayOnly = std::make_unique<DisplayOnlyCandidateList>();
        displayOnly->setContent(std::vector<std::string>{"你好"});
        plain.setCandidateList(std::move(displayOnly));
        setInlineCandidates(plain, {{"你好", "Hello"}});
        require(!plain.candidateList()->toPageable() &&
                    !plain.candidateList()->toBulk() &&
                    !plain.candidateList()->toModifiable() &&
                    !plain.candidateList()->toCursorMovable(),
                "adapter must not invent unsupported interfaces");
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Inline layout, paging, selection and configuration checks passed\n";
}
