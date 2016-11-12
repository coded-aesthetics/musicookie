# CMakeLists files in this project can
# refer to the root source directory of the project as ${NEARD_EXPLORENFC_SOURCE_DIR} and
# to the root binary directory of the project as ${NEARD_EXPLORENFC_BINARY_DIR}.
cmake_minimum_required (VERSION 2.8.11)

include(ExternalProject)
include(FindPkgConfig)
include(GNUInstallDirs)

project ( NEARD_EXPLORENFC )
set( PROJECT_VERSION 1.0 )

ExternalProject_Add (NXPRDLIBLINUX
SOURCE_DIR ${NEARD_EXPLORENFC_SOURCE_DIR}/nxprdlib-linux
PREFIX ${NEARD_EXPLORENFC_BINARY_DIR}/nxprdlib-linux
INSTALL_DIR ${NEARD_EXPLORENFC_BINARY_DIR}/nxprdlib-linux
CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${NEARD_EXPLORENFC_BINARY_DIR}/nxprdlib-linux
)
set(NXPRDLIBLINUX_SOURCE_DIR ${NEARD_EXPLORENFC_SOURCE_DIR}/nxprdlib-linux)
set(NXPRDLIBLINUX_LIB_DIR ${NEARD_EXPLORENFC_BINARY_DIR}/nxprdlib-linux/lib/static)

pkg_check_modules (GLIB glib-2.0>=2.40)
pkg_check_modules(GTHREAD gthread-2.0>=2.40)
pkg_check_modules(GIO gio-2.0>=2.40)
pkg_check_modules(GIOUNIX gio-unix-2.0>=2.40)
pkg_check_modules(NEARDAL neardal)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(config.h.in config.h)

set(INSTALL_CONFIG_DIR ${CMAKE_INSTALL_FULL_SYSCONFDIR}/neard-explorenfc/)

set(G_CFLAGS ${GLIB_CFLAGS} ${GTHREAD_CFLAGS} ${GIO_CFLAGS} ${GIOUNIX_CFLAGS})
set(G_LDFLAGS ${GLIB_LDFLAGS} ${GTHREAD_LDFLAGS} ${GIO_LDFLAGS} ${GIOUNIX_LDFLAGS})
add_subdirectory (src)
add_subdirectory (examples)
add_subdirectory (scripts)
