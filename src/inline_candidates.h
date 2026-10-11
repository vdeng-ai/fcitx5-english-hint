#pragma once

#include <fcitx/candidatelist.h>
#include <fcitx/inputpanel.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fcitx::english_hint {

using CandidateTranslations =
    std::vector<std::pair<std::string, std::string>>;

// Own the original list without changing its words or selection behavior.
// Optional Fcitx interfaces are exposed only if the original supports them.
class InlineCandidateList final : public CandidateList,
                                  public PageableCandidateList,
                                  public ModifiableCandidateList,
                                  public CursorMovableCandidateList {
public:
    explicit InlineCandidateList(std::shared_ptr<CandidateList> source);
    const std::shared_ptr<CandidateList> &source() const { return source_; }
    void setTranslations(CandidateTranslations translations, bool vertical);
    std::string translation(int index) const;

    const Text &label(int index) const override;
    const CandidateWord &candidate(int index) const override;
    int size() const override;
    int cursorIndex() const override;
    CandidateLayoutHint layoutHint() const override;
    bool hasPrev() const override;
    bool hasNext() const override;
    void prev() override;
    void next() override;
    bool usedNextBefore() const override;
    int totalPages() const override;
    int currentPage() const override;
    void setPage(int page) override;
    const CandidateWord &candidateFromAll(int index) const override;
    int totalSize() const override;
    void insert(int index, std::unique_ptr<CandidateWord> word) override;
    void remove(int index) override;
    void replace(int index, std::unique_ptr<CandidateWord> word) override;
    void move(int from, int to) override;
    void prevCandidate() override;
    void nextCandidate() override;

private:
    std::shared_ptr<CandidateList> source_;
    CandidateTranslations translations_;
    bool vertical_ = false;
    int page_ = -1;
    mutable std::vector<std::unique_ptr<CandidateWord>> words_;
};

std::shared_ptr<CandidateList> originalCandidates(const InputPanel &panel);
void clearInlineCandidates(InputPanel &panel);
void setInlineCandidates(InputPanel &panel, CandidateTranslations translations);

} // namespace fcitx::english_hint
