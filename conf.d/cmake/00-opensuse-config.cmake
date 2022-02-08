message(STATUS "Custom options: 00-opensuse-config.cmake --")

# Libshell is not part of standard Linux Distro (https://libshell.org/)
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/lib64/pkgconfig")

# memfd_create not present even on OpenSuse-15.2
add_definitions(-DMEMFD_CREATE_MISSING)

set(NODE_VERSION "node14")

# No pkgconfig for node-devel
if( NOT EXISTS "/usr/include/${NODE_VERSION}/node.h")
  message("  ### Nodejs development header Not-Found: install node-devel version=${NODE_VERSION}" )
  message(FATAL_ERROR "-------------------------------------------------------" )
endif()

set(EXTRA_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/../afb-libglue/src /usr/include/${NODE_VERSION})
