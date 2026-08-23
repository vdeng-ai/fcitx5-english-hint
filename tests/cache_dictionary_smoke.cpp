#include "local_dictionary.h"
#include "persistent_cache.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

int main() {
    std::string translation;
    if (!fcitx::english_hint::lookupLocalDictionary("提高效率", translation) ||
        translation != "improve efficiency") {
        std::cerr << "local dictionary hit failed\n";
        return 1;
    }
    if (fcitx::english_hint::lookupLocalDictionary("这句话应该交给模型", translation)) {
        std::cerr << "local dictionary unexpected hit\n";
        return 2;
    }

    const std::string path =
        "/tmp/fcitx5-english-hint-cache-smoke-" +
        std::to_string(static_cast<long long>(getpid())) + ".bin";
    std::error_code error;
    std::filesystem::remove(path, error);

    {
        fcitx::english_hint::PersistentCache cache(path);
        if (!cache.append({{"提高效率", "improve efficiency"},
                           {"测试", "testing"}})) {
            std::cerr << "persistent append failed\n";
            return 3;
        }
    }

    {
        fcitx::english_hint::PersistentCache cache(path);
        const auto entries = cache.load();
        if (entries.size() != 2 || entries[0].first != "提高效率" ||
            entries[0].second != "improve efficiency" ||
            entries[1].first != "测试" || entries[1].second != "testing") {
            std::cerr << "persistent load mismatch\n";
            return 4;
        }
        if (!cache.compact(entries)) {
            std::cerr << "persistent compact failed\n";
            return 5;
        }
    }

    const auto permissions = std::filesystem::status(path, error).permissions();
    if (error ||
        (permissions & (std::filesystem::perms::group_read |
                        std::filesystem::perms::others_read)) !=
            std::filesystem::perms::none) {
        std::cerr << "cache permissions are too broad\n";
        return 6;
    }

    std::filesystem::remove(path, error);
    std::cout << "cache + dictionary smoke: OK\n";
    return 0;
}
