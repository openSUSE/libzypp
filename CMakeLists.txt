# This project builds legacy and the next generation libzypp.
# Goal of having the same source tree for two generations of the library
# is that we can share code that does not need to be rewritten without having
# to rely on building another shared library and also be able to recompile the
# shared code with different compiler flags... e.g. coroutine support on/off

cmake_minimum_required(VERSION 3.17)

project(zypp-libs)

# use Boost's config file
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

# allow name libraries by name mixed with full
# paths
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

OPTION (ENABLE_BUILD_DOCS "Build documentation by default?" OFF)
OPTION (ENABLE_BUILD_TRANS "Build translation files by default?" OFF)
OPTION (ENABLE_BUILD_TESTS "Build and run test suite by default?" OFF)
OPTION (ENABLE_ZSTD_COMPRESSION "Build with zstd compression support?" OFF)
OPTION (ENABLE_VISIBILITY_HIDDEN "Build with hidden visibility by default?" OFF)
OPTION (ENABLE_ZCHUNK_COMPRESSION "Build with zchunk compression support?" OFF)
# Helps with bug https://bugzilla.gnome.org/show_bug.cgi?id=784550 , Segfault during signal emission when slots are cleared
OPTION (ENABLE_SIGC_BLOCK_WORKAROUND "Enable a workaround for older sigcpp libraries?" OFF )
OPTION (DISABLE_MEDIABACKEND_TESTS "Disable Tests depending on Nginx and libfcgi?" OFF)

OPTION (DISABLE_LIBPROXY "Build without libproxy support even if package is installed?" OFF)
OPTION (DISABLE_AUTODOCS "Do not require doxygen being installed (required to build autodocs)?" OFF)
# This option will reroute all tool binaries to the libzypp build dir instead of taking those installed in the default directories.
OPTION (ENABLE_DEVEL_BUILD "Developer build, use zypp tools directly from build dir rather than the default locations" OFF)
OPTION (ENABLE_CLANG_TIDY "Enables static checks with clang-tidy" OFF)
OPTION (ENABLE_CPPCHECK "Enables static checks with cppcheck" OFF)
OPTION (ENABLE_UBSAN_CHECKS "Enables the undefined behavior sanitizer runtime checks in debug builds." OFF)

OPTION (DISABLE_LIBZYPP_BUILD "Disable the build of the classic libzypp library" OFF)
OPTION (ENABLE_LIBZYPPNG_BUILD "Enable the build of the next gen libzypp library" OFF)
#--------------------------------------------------------------------------------
SET (have_system x)

IF (DEBIAN)
  MESSAGE (STATUS "Building for Debian")
  ADD_DEFINITIONS (-DDEBIAN)
  SET (ENABLE_BUILD_DOCS ON)
  SET (ENABLE_BUILD_TRANS ON)
  SET (ENABLE_BUILD_TESTS ON)
  SET (have_system ${have_system}x)
ENDIF (DEBIAN)

IF (SUSE)
  MESSAGE (STATUS "Building for SUSE (${LIBZYPP_CODESTREAM})")
  ADD_DEFINITIONS (-DSUSE)
  SET(LibSolv_USE_STATIC_LIBS ON)
  SET (ENABLE_BUILD_DOCS ON)
  SET (ENABLE_BUILD_TRANS ON)
  SET (ENABLE_BUILD_TESTS ON)
  SET (have_system ${have_system}x)
ENDIF (SUSE)

IF (${have_system} STREQUAL x)
  MESSAGE (STATUS "Building for no specific system type.")
ENDIF (${have_system} STREQUAL x)
IF (${have_system} STRGREATER xx)
  MESSAGE (FATAL_ERROR "Can only build for one system type.")
ENDIF (${have_system} STRGREATER xx)

IF ( NOSCHNICKSCHNACK )
  MESSAGE (STATUS "Building with NOSCHNICKSCHNACK")
  # In case you want make to build just the sources
  SET (ENABLE_BUILD_DOCS OFF)
  SET (ENABLE_BUILD_TRANS OFF)
  SET (ENABLE_BUILD_TESTS OFF)
ENDIF()
#--------------------------------------------------------------------------------


# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
SET( CMAKE_MODULE_PATH
    ${zypp-libs_SOURCE_DIR}/cmake/modules
    ${CMAKE_MODULE_PATH}
)

INCLUDE(ZyppCommon)

# we add the top level projects conditionally. This way we can exclude them from
# each others source tarball. ( not needed with _multibuild? )

if (NOT DISABLE_LIBZYPP_BUILD)
  add_subdirectory(zypp)
  MESSAGE(STATUS "ZyppConfig.cmake will be installed in ${LIB_INSTALL_DIR}/cmake/Zypp")
  INSTALL( FILES ${zypp-libs_SOURCE_DIR}/cmake/modules/ZyppConfig.cmake DESTINATION ${LIB_INSTALL_DIR}/cmake/Zypp )
  INSTALL( FILES ${zypp-libs_SOURCE_DIR}/cmake/modules/ZyppCommon.cmake DESTINATION ${LIB_INSTALL_DIR}/cmake/Zypp )
endif()

if (ENABLE_LIBZYPPNG_BUILD)
  OPTION (EXPORT_NG_API "Export experimental libzypp API" OFF)
  OPTION (INSTALL_NG_BINARIES "Installs the NG binaries, disabled for now since we are not actively using them." OFF)
  add_subdirectory(zyppng)
endif()
