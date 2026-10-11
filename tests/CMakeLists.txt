add_executable(english-hint-llm-smoke
    llm_smoke.cpp
    ../src/config.cpp
    ../src/llm_client.cpp
)

target_include_directories(english-hint-llm-smoke PRIVATE ../src)
target_compile_features(english-hint-llm-smoke PRIVATE cxx_std_17)
target_link_libraries(english-hint-llm-smoke PRIVATE
    Fcitx5::Config
    CURL::libcurl
)

add_executable(english-hint-cache-smoke
    cache_dictionary_smoke.cpp
    ../src/local_dictionary.cpp
    ../src/persistent_cache.cpp
)

target_include_directories(english-hint-cache-smoke PRIVATE ../src)
target_compile_features(english-hint-cache-smoke PRIVATE cxx_std_17)

add_test(NAME english-hint-cache-smoke
         COMMAND english-hint-cache-smoke)

add_executable(english-hint-aux-formatter-smoke
    aux_formatter_smoke.cpp
    ../src/aux_formatter.cpp
)

target_include_directories(english-hint-aux-formatter-smoke PRIVATE ../src)
target_compile_features(english-hint-aux-formatter-smoke PRIVATE cxx_std_17)

add_test(NAME english-hint-aux-formatter-smoke
         COMMAND english-hint-aux-formatter-smoke)

add_executable(english-hint-english-commit-smoke
    english_commit_smoke.cpp
    ../src/english_commit.cpp
    ../src/inline_candidates.cpp
    ../src/config.cpp
)
target_include_directories(english-hint-english-commit-smoke PRIVATE ../src)
target_compile_features(english-hint-english-commit-smoke PRIVATE cxx_std_17)
target_link_libraries(english-hint-english-commit-smoke PRIVATE
    Fcitx5::Core
    Fcitx5::Config
)
add_test(NAME english-hint-english-commit-smoke
         COMMAND english-hint-english-commit-smoke)

add_executable(english-hint-inline-candidates-smoke
    inline_candidates_smoke.cpp
    ../src/inline_candidates.cpp
    ../src/english_commit.cpp
    ../src/config.cpp
)
target_include_directories(english-hint-inline-candidates-smoke PRIVATE ../src)
target_compile_features(english-hint-inline-candidates-smoke PRIVATE cxx_std_17)
target_link_libraries(english-hint-inline-candidates-smoke PRIVATE
    Fcitx5::Core Fcitx5::Config)
add_test(NAME english-hint-inline-candidates-smoke
         COMMAND english-hint-inline-candidates-smoke)

option(ENGLISH_HINT_RIME_INTEGRATION "Test with an installed fcitx5-rime addon" OFF)
if(ENGLISH_HINT_RIME_INTEGRATION)
    add_executable(english-hint-rime-integration
        rime_integration.cpp
        ../src/addon.cpp
        ../src/aux_formatter.cpp
        ../src/config.cpp
        ../src/english_commit.cpp
        ../src/inline_candidates.cpp
        ../src/llm_client.cpp
        ../src/local_dictionary.cpp
        ../src/persistent_cache.cpp
        ../src/translation_worker.cpp
    )
    target_include_directories(english-hint-rime-integration PRIVATE ../src)
    target_compile_features(english-hint-rime-integration PRIVATE cxx_std_17)
    target_link_libraries(english-hint-rime-integration PRIVATE
        Fcitx5::Core Fcitx5::Config CURL::libcurl Threads::Threads)
    add_test(NAME english-hint-rime-integration COMMAND english-hint-rime-integration)
    set_tests_properties(english-hint-rime-integration PROPERTIES TIMEOUT 45)
endif()
