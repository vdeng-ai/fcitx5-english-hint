configure_file(english-hint.conf.in english-hint.conf @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/english-hint.conf"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/addon")
