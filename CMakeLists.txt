cmake_minimum_required(VERSION 3.16)

project(fcitx5-english-hint VERSION 0.7.0 LANGUAGES CXX)

set(REQUIRED_FCITX_VERSION 5.1.7)

find_package(ECM 1.0.0 REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_MODULE_PATH})
include(FeatureSummary)
include(GNUInstallDirs)
include(CTest)

find_package(Fcitx5Core ${REQUIRED_FCITX_VERSION} REQUIRED)
find_package(Fcitx5Config ${REQUIRED_FCITX_VERSION} REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

add_subdirectory(src)
add_subdirectory(data)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(FILES README.md CHANGELOG.md LICENSE CONTRIBUTING.md SECURITY.md
        DESTINATION "${CMAKE_INSTALL_DOCDIR}")
install(FILES config/english-hint.conf.example
        DESTINATION "${CMAKE_INSTALL_DOCDIR}/examples")

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

include(cmake/Packaging.cmake)
