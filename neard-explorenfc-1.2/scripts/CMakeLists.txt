configure_file(neard-explorenfc.init.in neard-explorenfc.init)
configure_file(neard-explorenfc.conf.in neard-explorenfc.conf)
configure_file(main.conf.in main.conf)

install(FILES ${NEARD_EXPLORENFC_BINARY_DIR}/scripts/neard-explorenfc.conf
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d/)
install(FILES ${NEARD_EXPLORENFC_BINARY_DIR}/scripts/main.conf
        DESTINATION ${INSTALL_CONFIG_DIR})
install(FILES ${NEARD_EXPLORENFC_BINARY_DIR}/scripts/neard-explorenfc.init
        DESTINATION ${CMAKE_INSTALL_DATADIR}/neard-explorenfc/ RENAME explorenfcd)