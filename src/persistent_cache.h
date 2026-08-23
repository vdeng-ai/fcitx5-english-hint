#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace fcitx::english_hint {

class PersistentCache {
public:
    explicit PersistentCache(std::string path);

    std::vector<std::pair<std::string, std::string>> load();
    bool append(const std::vector<std::pair<std::string, std::string>> &entries);
    bool shouldCompact() const;
    bool compact(
        const std::vector<std::pair<std::string, std::string>> &entries);

    const std::string &path() const { return path_; }

private:
    bool ensureParentDirectory() const;
    bool ensureHeader();

    std::string path_;
    std::size_t fileSize_ = 0;
};

std::string persistentCachePath();

} // namespace fcitx::english_hint
