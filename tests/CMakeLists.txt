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
