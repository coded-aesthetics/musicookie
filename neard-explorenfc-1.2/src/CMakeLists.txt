set( sources 
main.c 
dbus-daemon.c 
hal.c 
hal_tag.c 
hal_device.c 
adapter.c 
tag.c 
device.c 
record-container.c 
record.c 
ndef.c 
handover-agent.c 
generated-code.c
)

set( includes
  ${NEARD_EXPLORENFC_SOURCE_DIR}
  ${NXPRDLIBLINUX_SOURCE_DIR}/nxprdlib/NxpRdLib/intfs
  ${NXPRDLIBLINUX_SOURCE_DIR}/nxprdlib/NxpRdLib/types
  ${NXPRDLIBLINUX_SOURCE_DIR}/nxprdlib/NxpRdLib
  ${NXPRDLIBLINUX_SOURCE_DIR}/linux/intfs
  ${NXPRDLIBLINUX_SOURCE_DIR}/linux/comps/phbalReg/src/Linux
  ${NXPRDLIBLINUX_SOURCE_DIR}/linux/shared
)

set( definitions -D NXPBUILD_CUSTOMER_HEADER_INCLUDED -D DEBUG -D NATIVE_C_CODE -D LINUX 
-DCONFIGDIR="${INSTALL_CONFIG_DIR}"
-D NXPBUILD__PHHAL_HW_RC523
)
link_directories(${NXPRDLIBLINUX_LIB_DIR})

add_executable(explorenfcd  ${sources})

target_compile_options(explorenfcd PUBLIC "-pthread")
target_link_libraries (explorenfcd LINK_PUBLIC ${G_LDFLAGS} NxpRdLibLinuxPN512 rt ${CMAKE_THREAD_LIBS_INIT} pthread)
target_include_directories(explorenfcd PUBLIC ${includes})
target_compile_definitions(explorenfcd PUBLIC ${definitions})

add_definitions(-std=gnu99 -pthread ${G_CFLAGS})

install(TARGETS explorenfcd
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static)
        