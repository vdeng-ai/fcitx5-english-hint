#pragma once

#include <cstddef>
#include <cstdint>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fcitx::english_hint {

class TranslationCache {
public:
    explicit TranslationCache(std::size_t capacity) : capacity_(capacity) {}

    void setCapacity(std::size_t capacity) {
        std::lock_guard<std::mutex> lock(mutex_);
        capacity_ = capacity;
        while (items_.size() > capacity_) {
            entries_.erase(items_.back().first);
            items_.pop_back();
        }
    }

    bool get(const std::string &key, std::string &value) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = entries_.find(key);
        if (it == entries_.end()) {
            ++misses_;
            return false;
        }
        ++hits_;
        items_.splice(items_.begin(), items_, it->second);
        value = it->second->second;
        return true;
    }

    void put(std::string key, std::string value) {
        if (key.empty() || value.empty()) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (const auto it = entries_.find(key); it != entries_.end()) {
            it->second->second = std::move(value);
            items_.splice(items_.begin(), items_, it->second);
            return;
        }

        items_.emplace_front(std::move(key), std::move(value));
        entries_[items_.front().first] = items_.begin();

        while (items_.size() > capacity_) {
            entries_.erase(items_.back().first);
            items_.pop_back();
        }
    }

    std::vector<std::pair<std::string, std::string>> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::pair<std::string, std::string>> result;
        result.reserve(items_.size());
        for (auto it = items_.rbegin(); it != items_.rend(); ++it) {
            result.push_back(*it);
        }
        return result;
    }

    std::pair<std::uint64_t, std::uint64_t> stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {hits_, misses_};
    }

private:
    using ItemList = std::list<std::pair<std::string, std::string>>;

    std::size_t capacity_;
    ItemList items_;
    std::unordered_map<std::string, ItemList::iterator> entries_;
    mutable std::mutex mutex_;
    std::uint64_t hits_ = 0;
    std::uint64_t misses_ = 0;
};

} // namespace fcitx::english_hint
