#include "inline_candidates.h"

#include <fcitx-utils/textformatflags.h>

namespace fcitx::english_hint {
namespace {

class InlineCandidate final : public CandidateWord {
public:
    InlineCandidate(std::shared_ptr<CandidateList> source, int index,
                    const std::string &translation)
        : source_(std::move(source)), index_(index) {
        const auto &original = source_->candidate(index_);
        Text text = original.text();
        text.append("  " + translation, TextFormatFlag::DontCommit);
        setText(std::move(text));
        setPlaceHolder(original.isPlaceHolder());
        if (original.hasCustomLabel()) {
            setCustomLabel(original.customLabel());
        }
    }

    void select(InputContext *inputContext) const override {
        // Selection can synchronously replace the panel and destroy this word.
        // Keep the original list alive across its select() call.
        auto source = source_;
        const int index = index_;
        source->candidate(index).select(inputContext);
    }

private:
    std::shared_ptr<CandidateList> source_;
    int index_;
};

} // namespace

InlineCandidateList::InlineCandidateList(std::shared_ptr<CandidateList> source)
    : source_(std::move(source)) {
    if (source_->toPageable()) {
        setPageable(this);
    }
    if (source_->toBulk()) {
        setBulk(this);
    }
    if (source_->toModifiable()) {
        setModifiable(this);
    }
    if (source_->toCursorMovable()) {
        setCursorMovable(this);
    }
}

void InlineCandidateList::setTranslations(CandidateTranslations translations,
                                         bool vertical) {
    translations_ = std::move(translations);
    vertical_ = vertical;
    page_ = currentPage();
    words_.clear();
}

std::string InlineCandidateList::translation(int index) const {
    if (!vertical_ || index < 0 || index >= size() || currentPage() != page_ ||
        static_cast<std::size_t>(index) >= translations_.size()) {
        return {};
    }
    const auto &word = source_->candidate(index);
    const auto &[source, english] = translations_[index];
    return !word.isPlaceHolder() && word.text().toString() == source
               ? english
               : std::string{};
}

const Text &InlineCandidateList::label(int index) const {
    return source_->label(index);
}
const CandidateWord &InlineCandidateList::candidate(int index) const {
    const auto english = translation(index);
    if (english.empty()) {
        return source_->candidate(index);
    }
    if (words_.size() <= static_cast<std::size_t>(index)) {
        words_.resize(index + 1);
    }
    if (!words_[index]) {
        words_[index] = std::make_unique<InlineCandidate>(source_, index, english);
    }
    return *words_[index];
}
int InlineCandidateList::size() const { return source_->size(); }
int InlineCandidateList::cursorIndex() const { return source_->cursorIndex(); }
CandidateLayoutHint InlineCandidateList::layoutHint() const {
    return vertical_ ? CandidateLayoutHint::Vertical : source_->layoutHint();
}
bool InlineCandidateList::hasPrev() const {
    return source_->toPageable()->hasPrev();
}
bool InlineCandidateList::hasNext() const {
    return source_->toPageable()->hasNext();
}
void InlineCandidateList::prev() {
    translations_.clear();
    words_.clear();
    auto source = source_;
    source->toPageable()->prev();
}
void InlineCandidateList::next() {
    translations_.clear();
    words_.clear();
    auto source = source_;
    source->toPageable()->next();
}
bool InlineCandidateList::usedNextBefore() const {
    return source_->toPageable()->usedNextBefore();
}
int InlineCandidateList::totalPages() const {
    auto *pageable = source_->toPageable();
    return pageable ? pageable->totalPages() : -1;
}
int InlineCandidateList::currentPage() const {
    auto *pageable = source_->toPageable();
    return pageable ? pageable->currentPage() : -1;
}
void InlineCandidateList::setPage(int page) {
    translations_.clear();
    words_.clear();
    auto source = source_;
    source->toPageable()->setPage(page);
}
const CandidateWord &InlineCandidateList::candidateFromAll(int index) const {
    // Bulk indices are global, while translations are page-local. Leave bulk
    // words unchanged for virtual keyboards and candidate-editing addons.
    return source_->toBulk()->candidateFromAll(index);
}
int InlineCandidateList::totalSize() const {
    return source_->toBulk()->totalSize();
}
void InlineCandidateList::insert(int index, std::unique_ptr<CandidateWord> word) {
    translations_.clear();
    words_.clear();
    auto source = source_;
    source->toModifiable()->insert(index, std::move(word));
}
void InlineCandidateList::remove(int index) {
    translations_.clear();
    words_.clear();
    auto source = source_;
    source->toModifiable()->remove(index);
}
void InlineCandidateList::replace(int index, std::unique_ptr<CandidateWord> word) {
    translations_.clear();
    words_.clear();
    auto source = source_;
    source->toModifiable()->replace(index, std::move(word));
}
void InlineCandidateList::move(int from, int to) {
    translations_.clear();
    words_.clear();
    auto source = source_;
    source->toModifiable()->move(from, to);
}
void InlineCandidateList::prevCandidate() {
    auto source = source_;
    source->toCursorMovable()->prevCandidate();
}
void InlineCandidateList::nextCandidate() {
    auto source = source_;
    source->toCursorMovable()->nextCandidate();
}

std::shared_ptr<CandidateList> originalCandidates(const InputPanel &panel) {
    auto list = panel.candidateList();
    if (auto decorated = std::dynamic_pointer_cast<InlineCandidateList>(list)) {
        return decorated->source();
    }
    return list;
}
void clearInlineCandidates(InputPanel &panel) {
    if (auto decorated = std::dynamic_pointer_cast<InlineCandidateList>(
            panel.candidateList())) {
        // InputPanel accepts unique_ptr only, so retain a transparent adapter
        // until Rime replaces the list. Original word identities are restored.
        decorated->setTranslations({}, false);
    }
}
void setInlineCandidates(InputPanel &panel, CandidateTranslations translations) {
    auto list = panel.candidateList();
    if (auto decorated = std::dynamic_pointer_cast<InlineCandidateList>(list)) {
        decorated->setTranslations(std::move(translations), true);
    } else if (list) {
        auto decorated = std::make_unique<InlineCandidateList>(std::move(list));
        decorated->setTranslations(std::move(translations), true);
        panel.setCandidateList(std::move(decorated));
    }
}

} // namespace fcitx::english_hint
