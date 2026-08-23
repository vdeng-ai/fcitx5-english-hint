add_library(english-hint MODULE
    addon.cpp
    config.cpp
    llm_client.cpp
    local_dictionary.cpp
    persistent_cache.cpp
    translation_worker.cpp
)

target_compile_features(english-hint PRIVATE cxx_std_17)
target_link_libraries(english-hint PRIVATE
    Fcitx5::Core
    Fcitx5::Config
    CURL::libcurl
    Threads::Threads
)

install(TARGETS english-hint DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")
