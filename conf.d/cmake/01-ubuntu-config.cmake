message(STATUS "Custom options: 00-ubuntu-config.cmake --")

# Libshell is not part of standard Linux Distro (https://libshell.org/)
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib64/pkgconfig")

# No pkgconfig for node-devel
if( NOT EXISTS "/usr/include/node/node.h")
  message("  ### Nodejs development header Not-Found: install node-devel" )
  message(FATAL_ERROR "----------------" )
endif()
