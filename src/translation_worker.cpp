#include "translation_worker.h"

#include <fcitx-utils/log.h>

#include <algorithm>
#include <chrono>
#include <unordered_set>
#include <utility>

namespace fcitx::english_hint {

FCITX_DEFINE_LOG_CATEGORY(english_hint_worker_log, "english-hint.worker");

TranslationWorker::TranslationWorker(EnglishHintConfig config,
                                     TranslationCache &cache,
                                     PersistentCache *persistentCache,
                                     EventDispatcher &dispatcher,
                                     std::function<void()> refreshCallback)
    : config_(std::move(config)), cache_(cache),
      persistentCache_(persistentCache), dispatcher_(dispatcher),
      refreshCallback_(std::move(refreshCallback)), client_(config_),
      lastSubmit_(std::chrono::steady_clock::now()),
      thread_(&TranslationWorker::run, this) {}

TranslationWorker::~TranslationWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    condition_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

std::string
TranslationWorker::signature(const std::vector<std::string> &candidates) {
    std::string result;
    std::size_t bytes = 0;
    for (const auto &candidate : candidates) {
        bytes += candidate.size() + 1;
    }
    result.reserve(bytes);
    for (const auto &candidate : candidates) {
        result.append(candidate);
        result.push_back('\x1f');
    }
    return result;
}

bool TranslationWorker::submit(std::vector<std::string> candidates) {
    if (candidates.empty()) {
        return false;
    }

    if (candidates.size() > config_.maxBatch) {
        candidates.resize(config_.maxBatch);
    }

    std::unordered_set<std::string> seen;
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
                       [&seen](const std::string &candidate) {
                           return candidate.empty() || !seen.insert(candidate).second;
                       }),
        candidates.end());
    if (candidates.empty()) {
        return false;
    }

    const std::string newSignature = signature(candidates);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_ || newSignature == pendingSignature_ ||
            newSignature == inFlightSignature_) {
            return false;
        }

        // IME semantics are latest-wins: a newer candidate page replaces any
        // waiting page rather than growing a FIFO queue.
        pending_ = std::move(candidates);
        pendingSignature_ = newSignature;
        hasPending_ = true;
        pendingGeneration_ = latestGeneration_.fetch_add(1) + 1;
        lastSubmit_ = std::chrono::steady_clock::now();
    }
    condition_.notify_one();
    return true;
}

void TranslationWorker::run() {
    std::unique_lock<std::mutex> lock(mutex_);

    while (!stopping_) {
        condition_.wait(lock, [this] { return stopping_ || hasPending_; });
        if (stopping_) {
            break;
        }

        // Debounce while allowing a newer page to replace the pending page.
        while (!stopping_) {
            const auto observedSubmit = lastSubmit_;
            const auto deadline =
                observedSubmit + std::chrono::milliseconds(config_.debounceMs);
            condition_.wait_until(lock, deadline, [this, observedSubmit] {
                return stopping_ || lastSubmit_ != observedSubmit;
            });
            if (stopping_) {
                break;
            }
            if (lastSubmit_ == observedSubmit) {
                break;
            }
        }
        if (stopping_) {
            break;
        }

        std::vector<std::string> batch = std::move(pending_);
        const std::string batchSignature = std::move(pendingSignature_);
        const std::uint64_t generation = pendingGeneration_;
        pending_.clear();
        pendingSignature_.clear();
        hasPending_ = false;
        inFlightSignature_ = batchSignature;

        lock.unlock();
        const auto started = std::chrono::steady_clock::now();
        const auto translations = client_.translate(batch);
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - started)
                                   .count();
        ++requestCount_;

        bool anyTranslation = false;
        for (const auto &translation : translations) {
            if (!translation.empty()) {
                anyTranslation = true;
                break;
            }
        }
        if (!anyTranslation) {
            ++failureCount_;
        }

        const bool stale = latestGeneration_.load() != generation;
        if (stale) {
            ++staleCount_;
        }

        // Old results are still useful cache entries, but never trigger an old
        // UI refresh. This avoids stale candidate pages flashing back on screen.
        bool changed = false;
        std::vector<std::pair<std::string, std::string>> newEntries;
        newEntries.reserve(batch.size());
        for (std::size_t i = 0; i < batch.size() && i < translations.size();
             ++i) {
            if (!translations[i].empty()) {
                cache_.put(batch[i], translations[i]);
                newEntries.emplace_back(batch[i], translations[i]);
                changed = true;
            }
        }

        if (persistentCache_ && !newEntries.empty()) {
            persistentCache_->append(newEntries);
            if (persistentCache_->shouldCompact()) {
                persistentCache_->compact(cache_.snapshot());
            }
        }

        if (config_.debug) {
            const auto [cacheHits, cacheMisses] = cache_.stats();
            FCITX_INFO() << "english-hint request=" << requestCount_
                         << " batch=" << batch.size() << " latency="
                         << elapsedMs << "ms stale=" << stale
                         << " failures=" << failureCount_
                         << " stale_total=" << staleCount_
                         << " cache_hits=" << cacheHits
                         << " cache_misses=" << cacheMisses;
        }

        lock.lock();
        if (inFlightSignature_ == batchSignature) {
            inFlightSignature_.clear();
        }
        lock.unlock();

        if (changed && !stale) {
            dispatcher_.schedule(refreshCallback_);
        }

        lock.lock();
    }
}

} // namespace fcitx::english_hint
