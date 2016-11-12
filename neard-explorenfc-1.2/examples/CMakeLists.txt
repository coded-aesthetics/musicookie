set( includes
  ${NEARD_EXPLORENFC_SOURCE_DIR}
)

set( definitions 
-DCONFIGDIR="${INSTALL_CONFIG_DIR}"
)

add_executable(explorenfc-basic basic.c)
target_link_libraries (explorenfc-basic LINK_PUBLIC ${G_LDFLAGS} ${NEARDAL_LDFLAGS})
target_include_directories(explorenfc-basic PUBLIC ${includes})
target_compile_definitions(explorenfc-basic PUBLIC ${definitions})

add_executable(explorenfc-browser browser.c)
target_link_libraries (explorenfc-browser LINK_PUBLIC ${G_LDFLAGS} ${NEARDAL_LDFLAGS})
target_include_directories(explorenfc-browser PUBLIC ${includes})
target_compile_definitions(explorenfc-browser PUBLIC ${definitions})

add_executable(explorenfc-wifi-connect wifi-connect.c wifi-connect-wsc-parser.c wifi-connect-dbus-gen.c)
target_link_libraries (explorenfc-wifi-connect LINK_PUBLIC ${G_LDFLAGS} ${NEARDAL_LDFLAGS})
target_include_directories(explorenfc-wifi-connect PUBLIC ${includes})
target_compile_definitions(explorenfc-wifi-connect PUBLIC ${definitions})

add_definitions(-std=gnu99 ${G_CFLAGS} ${NEARDAL_CFLAGS})

install(TARGETS explorenfc-basic explorenfc-browser explorenfc-wifi-connect
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static)