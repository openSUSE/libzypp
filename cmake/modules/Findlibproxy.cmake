# The "real" libproxy provides its own Findlibproxy.cmake but saner, simpler
# alternatives like the PacRunner replacement which *just* queries PacRunner
# directly will only provide a .pc file. So use pkg-config to find it...

if(NOT PKG_CONFIG_FOUND)
  include(FindPkgConfig)
endif()

PKG_CHECK_MODULES( LIBPROXY libproxy-1.0 )
