cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(zeek-client)

set(ZEEK_BIN_DIR ${CMAKE_INSTALL_PREFIX}/bin)
set(orig_file ${CMAKE_CURRENT_SOURCE_DIR}/zeek-client)
set(configed_file ${CMAKE_CURRENT_BINARY_DIR}/zeek-client)
configure_file(${orig_file} ${configed_file} @ONLY)

# Install zeek-client
install(DIRECTORY DESTINATION bin)
install(PROGRAMS ${configed_file} DESTINATION bin)

if ( NOT PY_MOD_INSTALL_DIR )
    # This is not a Zeek-bundled install. Default to "home"-style install.
    set(PY_MOD_INSTALL_DIR lib/python)
endif ()

message(
    "\n==================|  zeek-client Build Summary  |==============="
    "\n"
    "\nInstall prefix:      ${CMAKE_INSTALL_PREFIX}"
    "\n"
    "\n================================================================\n"
)
