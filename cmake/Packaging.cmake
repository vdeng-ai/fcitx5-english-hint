set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "fcitx5-english-hint")
set(CPACK_PACKAGE_VENDOR "vdeng-ai")
set(CPACK_PACKAGE_CONTACT "vdeng-ai")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Lightweight English translation hints for Fcitx5 Rime candidates")
set(CPACK_PACKAGE_HOMEPAGE_URL
    "https://github.com/vdeng-ai/fcitx5-english-hint")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

# Official binary package target: Ubuntu 24.04 amd64 only.
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "fcitx5 (>= 5.1.7), fcitx5-rime (>= 5.1.4)")

include(CPack)
