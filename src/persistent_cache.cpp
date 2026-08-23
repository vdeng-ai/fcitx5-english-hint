#include "persistent_cache.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fcitx::english_hint {
namespace {

constexpr std::array<char, 8> kMagic = {'E', 'H', 'C', 'A', 'C', 'H', 'E', '1'};
constexpr std::uint32_t kMaxFieldBytes = 4096;
constexpr std::size_t kCompactThresholdBytes = 4 * 1024 * 1024;

void writeU32(std::ostream &stream, std::uint32_t value) {
    const std::array<char, 4> bytes = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff),
        static_cast<char>((value >> 24) & 0xff),
    };
    stream.write(bytes.data(), bytes.size());
}

bool readU32(std::istream &stream, std::uint32_t &value) {
    std::array<unsigned char, 4> bytes{};
    stream.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
    if (!stream) {
        return false;
    }
    value = static_cast<std::uint32_t>(bytes[0]) |
            (static_cast<std::uint32_t>(bytes[1]) << 8) |
            (static_cast<std::uint32_t>(bytes[2]) << 16) |
            (static_cast<std::uint32_t>(bytes[3]) << 24);
    return true;
}

bool writeRecord(std::ostream &stream, const std::string &key,
                 const std::string &value) {
    if (key.empty() || value.empty() || key.size() > kMaxFieldBytes ||
        value.size() > kMaxFieldBytes) {
        return false;
    }
    writeU32(stream, static_cast<std::uint32_t>(key.size()));
    writeU32(stream, static_cast<std::uint32_t>(value.size()));
    stream.write(key.data(), static_cast<std::streamsize>(key.size()));
    stream.write(value.data(), static_cast<std::streamsize>(value.size()));
    return static_cast<bool>(stream);
}

void restrictPermissions(const std::string &path) {
    std::error_code error;
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace, error);
}

} // namespace

PersistentCache::PersistentCache(std::string path) : path_(std::move(path)) {}

bool PersistentCache::ensureParentDirectory() const {
    if (path_.empty()) {
        return false;
    }
    std::error_code error;
    const auto parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return false;
        }
    }
    return true;
}

bool PersistentCache::ensureHeader() {
    if (!ensureParentDirectory()) {
        return false;
    }

    std::error_code error;
    const bool exists = std::filesystem::exists(path_, error) && !error;
    const auto size = exists ? std::filesystem::file_size(path_, error) : 0;
    if (exists && !error && size >= kMagic.size()) {
        std::ifstream input(path_, std::ios::binary);
        std::array<char, kMagic.size()> magic{};
        input.read(magic.data(), magic.size());
        if (input && magic == kMagic) {
            fileSize_ = static_cast<std::size_t>(size);
            return true;
        }
    }

    std::ofstream output(path_, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.write(kMagic.data(), kMagic.size());
    output.close();
    if (!output) {
        return false;
    }
    restrictPermissions(path_);
    fileSize_ = kMagic.size();
    return true;
}

std::vector<std::pair<std::string, std::string>> PersistentCache::load() {
    std::vector<std::pair<std::string, std::string>> result;
    if (path_.empty()) {
        return result;
    }

    std::error_code error;
    if (!std::filesystem::exists(path_, error) || error) {
        return result;
    }

    std::ifstream input(path_, std::ios::binary);
    if (!input) {
        return result;
    }

    std::array<char, kMagic.size()> magic{};
    input.read(magic.data(), magic.size());
    if (!input || magic != kMagic) {
        return result;
    }

    while (input) {
        std::uint32_t keySize = 0;
        std::uint32_t valueSize = 0;
        if (!readU32(input, keySize)) {
            break;
        }
        if (!readU32(input, valueSize)) {
            break;
        }
        if (keySize == 0 || valueSize == 0 || keySize > kMaxFieldBytes ||
            valueSize > kMaxFieldBytes) {
            break;
        }

        std::string key(keySize, '\0');
        std::string value(valueSize, '\0');
        input.read(key.data(), static_cast<std::streamsize>(key.size()));
        input.read(value.data(), static_cast<std::streamsize>(value.size()));
        if (!input) {
            break;
        }
        result.emplace_back(std::move(key), std::move(value));
    }

    const auto size = std::filesystem::file_size(path_, error);
    if (!error) {
        fileSize_ = static_cast<std::size_t>(size);
    }
    return result;
}

bool PersistentCache::append(
    const std::vector<std::pair<std::string, std::string>> &entries) {
    if (entries.empty()) {
        return true;
    }
    if (!ensureHeader()) {
        return false;
    }

    std::ofstream output(path_, std::ios::binary | std::ios::app);
    if (!output) {
        return false;
    }

    std::size_t appendedBytes = 0;
    for (const auto &[key, value] : entries) {
        if (writeRecord(output, key, value)) {
            appendedBytes += 8 + key.size() + value.size();
        }
    }
    output.close();
    if (!output) {
        return false;
    }

    restrictPermissions(path_);
    fileSize_ += appendedBytes;
    return true;
}

bool PersistentCache::shouldCompact() const {
    return fileSize_ > kCompactThresholdBytes;
}

bool PersistentCache::compact(
    const std::vector<std::pair<std::string, std::string>> &entries) {
    if (!ensureParentDirectory()) {
        return false;
    }

    const std::string temporaryPath = path_ + ".tmp";
    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.write(kMagic.data(), kMagic.size());
    for (const auto &[key, value] : entries) {
        writeRecord(output, key, value);
    }
    output.close();
    if (!output) {
        std::error_code ignore;
        std::filesystem::remove(temporaryPath, ignore);
        return false;
    }
    restrictPermissions(temporaryPath);

    std::error_code error;
    std::filesystem::rename(temporaryPath, path_, error);
    if (error) {
        std::filesystem::remove(temporaryPath, error);
        return false;
    }
    restrictPermissions(path_);

    const auto size = std::filesystem::file_size(path_, error);
    fileSize_ = error ? 0 : static_cast<std::size_t>(size);
    return !error;
}

std::string persistentCachePath() {
    if (const char *xdg = std::getenv("XDG_CACHE_HOME"); xdg && *xdg) {
        return std::string(xdg) + "/fcitx5-english-hint/cache.bin";
    }
    if (const char *home = std::getenv("HOME"); home && *home) {
        return std::string(home) + "/.cache/fcitx5-english-hint/cache.bin";
    }
    return {};
}

} // namespace fcitx::english_hint
