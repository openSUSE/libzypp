# This project builds legacy and the next generation libzypp.
# Goal of having the same source tree for two generations of the library
# is that we can share code that does not need to be rewritten without having
# to rely on building another shared library and also be able to recompile the
# shared code with different compiler flags... e.g. coroutine support on/off

cmake_minimum_required(VERSION 3.17)

project(zypp-libs)

INCLUDE(CMakePushCheckState)
include(CheckSymbolExists)

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
OPTION (ENABLE_VISIBILITY_HIDDEN "Build with hidden visibility by default?" ON)
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

OPTION (BUILD_LIBZYPP "Build of the classic libzypp library" ON)
OPTION (BUILD_LIBZYPPNG "Build of the next gen libzypp library" ON)
OPTION (KEEP_LEGACY_ZYPPCONF "Install the vendor zypp.conf in /etc too (ONLY LEGACY SYSTEMS WANT THIS)" OFF)
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
# We're going to move our default configuration successively into /usr/etc/.
# The final configuration data will then be merged according to the rules
# defined by the UAPI.6 Configuration Files Specification from:
#   system-wide configuration (/etc    /zypp/zypp.conf[.d/*.conf])
#   vendor configuration      (/usr/etc/zypp/zypp.conf[.d/*.conf])
IF( NOT DEFINED ZYPPCONFDIR )
  SET ( ZYPPCONFDIR "/usr/etc" )
ENDIF()
#--------------------------------------------------------------------------------

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
SET( CMAKE_MODULE_PATH
    ${zypp-libs_SOURCE_DIR}/cmake/modules
    ${CMAKE_MODULE_PATH}
)
INCLUDE(ZyppCommon)

# Macro to set the log group for a list of files
MACRO( SET_LOGGROUP _group _files  )
  SET_SOURCE_FILES_PROPERTIES( ${_files} COMPILE_FLAGS -DZYPP_BASE_LOGGER_LOGGROUP=\\"${_group}\\" )
  FOREACH (_currentFile ${ARGN})
#MESSAGE( STATUS "setting loggroup to \"${_group}\" for ${_currentFile}" )
    SET_SOURCE_FILES_PROPERTIES( ${_currentFile} COMPILE_FLAGS -DZYPP_BASE_LOGGER_LOGGROUP=\\"${_group}\\" )
  ENDFOREACH (_currentFile ${ARGN})
ENDMACRO( SET_LOGGROUP )

# general flags used for all targets
add_library( zypp_initial_compiler_flags INTERFACE )

SET( CMAKE_THREAD_PREFER_PTHREAD TRUE )
FIND_PACKAGE( Threads REQUIRED )
IF ( NOT CMAKE_USE_PTHREADS_INIT )
  MESSAGE( FATAL_ERROR "No pthreads found" )
ENDIF ( NOT CMAKE_USE_PTHREADS_INIT )

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
CHECK_C_COMPILER_FLAG("-Werror=format-security" CC_FORMAT_SECURITY)
CHECK_CXX_COMPILER_FLAG("-Werror=format-security" CXX_FORMAT_SECURITY)

IF(${CXX_FORMAT_SECURITY})
  target_compile_options( zypp_initial_compiler_flags INTERFACE "-Werror=format-security")
  target_link_options( zypp_initial_compiler_flags INTERFACE "-Werror=format-security")
ENDIF(${CXX_FORMAT_SECURITY})

target_compile_options( zypp_initial_compiler_flags INTERFACE "-pthread" "-fno-strict-aliasing" "-g" "-Wall" "-Wp,-D_GLIBCXX_ASSERTIONS" )
target_link_options( zypp_initial_compiler_flags INTERFACE "-pthread" "-fno-strict-aliasing" "-g" "-Wall" "-Wp,-D_GLIBCXX_ASSERTIONS" )
target_compile_options( zypp_initial_compiler_flags INTERFACE "-fvisibility-inlines-hidden" "-Woverloaded-virtual" "-Wnon-virtual-dtor" "-ftemplate-backtrace-limit=0" )
target_link_options( zypp_initial_compiler_flags INTERFACE "-fvisibility-inlines-hidden" "-Woverloaded-virtual" "-Wnon-virtual-dtor" "-ftemplate-backtrace-limit=0" )

#cli args ignored by clang, it still prints a noisy warning though
if ( NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  target_compile_options( zypp_initial_compiler_flags INTERFACE "-Wl,-as-needed" )
  target_link_options( zypp_initial_compiler_flags    INTERFACE "-Wl,-as-needed" )

  if( ENABLE_UBSAN_CHECKS )
    target_compile_options( zypp_initial_compiler_flags INTERFACE "$<$<CONFIG:DEBUG>:-fsanitize=undefined>" )
    target_link_options( zypp_initial_compiler_flags    INTERFACE "$<$<CONFIG:DEBUG>:-fsanitize=undefined>" )
  endif()
endif()

INCLUDE(CheckFunctionExists)
CHECK_FUNCTION_EXISTS(pipe2 PIPE2_FOUND)
IF(${PIPE2_FOUND})
  target_compile_definitions( zypp_initial_compiler_flags INTERFACE HAVE_PIPE2 )
ENDIF(${PIPE2_FOUND})

#tell libsolv to rename its variables to be C++20 compatible
target_compile_definitions(zypp_initial_compiler_flags INTERFACE LIBSOLV_SOLVABLE_PREPEND_DEP )
target_compile_definitions(zypp_initial_compiler_flags INTERFACE _FILE_OFFSET_BITS=64 LIBSOLV_SOLVABLE_PREPEND_DEP )
target_compile_definitions(zypp_initial_compiler_flags INTERFACE $<$<NOT:$<CONFIG:DEBUG>>:ZYPP_NDEBUG NDEBUG> )

target_compile_options( zypp_initial_compiler_flags INTERFACE $<$<NOT:$<CONFIG:DEBUG>>:-O2>)
target_link_options   ( zypp_initial_compiler_flags INTERFACE $<$<NOT:$<CONFIG:DEBUG>>:-O2>)

find_package(FindPkgConfig, QUIET)

FIND_PACKAGE(Boost REQUIRED COMPONENTS program_options unit_test_framework thread)
IF (Boost_FOUND)
  MESSAGE( STATUS "boost found: includes in ${Boost_INCLUDE_DIRS}, library in ${Boost_LIBRARY_DIRS}")
  target_include_directories( zypp_initial_compiler_flags INTERFACE ${Boost_INCLUDE_DIRS})
  target_link_directories( zypp_initial_compiler_flags INTERFACE ${Boost_LIBRARY_DIRS})
ENDIF(Boost_FOUND)

FIND_PACKAGE(Gettext REQUIRED)
IF (GETTEXT_FOUND)
  MESSAGE(STATUS "Found Gettext: ${GETTEXT_SOURCE}")
  target_include_directories( zypp_initial_compiler_flags INTERFACE ${GETTEXT_INCLUDE_DIR})
ELSE (GETTEXT_FOUND)
  MESSAGE( FATAL_ERROR "Gettext not found" )
ENDIF (GETTEXT_FOUND)

FIND_PACKAGE(CURL REQUIRED)
IF ( NOT CURL_FOUND)
  MESSAGE( FATAL_ERROR " curl not found" )
ELSE ( NOT CURL_FOUND)
  target_include_directories( zypp_initial_compiler_flags INTERFACE ${CURL_INCLUDE_DIRS})
ENDIF( NOT CURL_FOUND)

FIND_PACKAGE(LibXml2 REQUIRED)
IF ( NOT LIBXML2_FOUND)
  MESSAGE( FATAL_ERROR " libxml not found" )
ELSE ( NOT LIBXML2_FOUND)
  target_include_directories( zypp_initial_compiler_flags INTERFACE ${LIBXML2_INCLUDE_DIR})

  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_INCLUDES ${LIBXML2_INCLUDE_DIR} ${CMAKE_REQUIRED_INCLUDES})
  set(CMAKE_REQUIRED_LIBRARIES ${LIBXML2_LIBRARIES} )
  set(CMAKE_REQUIRED_QUIET FALSE )
  check_symbol_exists( xmlCtxtSetErrorHandler "libxml/SAX2.h" XMLCTXTSETERRORHANDLER_FOUND )
  IF(${XMLCTXTSETERRORHANDLER_FOUND})
    target_compile_definitions( zypp_initial_compiler_flags INTERFACE HAVE_LIBXML2_XMLCTXTSETERRORHANDLER )
  ENDIF()
  cmake_pop_check_state()

ENDIF( NOT LIBXML2_FOUND)

FIND_PACKAGE(ZLIB REQUIRED)
IF ( NOT ZLIB_FOUND)
  MESSAGE( FATAL_ERROR " zlib not found" )
ELSE ( NOT ZLIB_FOUND)
  target_include_directories( zypp_initial_compiler_flags INTERFACE ${ZLIB_INCLUDE_DIR})
ENDIF( NOT ZLIB_FOUND)

FIND_PACKAGE(YAML-CPP REQUIRED)
IF ( NOT YAML-CPP_FOUND)
  MESSAGE( FATAL_ERROR " yaml-cpp not found" )
ELSE ( NOT YAML-CPP_FOUND)
  target_include_directories( zypp_initial_compiler_flags INTERFACE ${YAML_CPP_INCLUDE_DIR})
  IF ( NOT YAML_CPP_LIBRARIES )
    # Must have been found by config insted of module, use target instead.
    SET( YAML_CPP_LIBRARIES "yaml-cpp" )
  ENDIF ( NOT YAML_CPP_LIBRARIES )
ENDIF( NOT YAML-CPP_FOUND)

#Allow to override the libsolv install location
if( ZYPP_STACK_BUILD )
  set ( LibSolv_FOUND TRUE )
  SET ( LibSolv_INCLUDE_DIRS ${ZYPP_STACK_INCLUDE_DIRS} )
  SET ( LibSolv_LIBRARIES libsolvext libsolv )
else()
  if ( LibSolv_USE_STATIC_LIBS )
    MESSAGE( STATUS "Require static libsolv libraries..." )
  endif()
  FIND_PACKAGE(LibSolv REQUIRED ext)
  IF ( NOT LibSolv_FOUND )
    MESSAGE( FATAL_ERROR " libsolv not found" )
  ELSE()
    # static case: Assert libsolv.a ist the last one linked
    SET(LibSolv_LIBRARIES ${LibSolv_LIBRARIES} ${LIBSOLV_LIBRARY})
  ENDIF()

  # detecting libsolv features, so we know which libraries to link this is still required even if we were asked to build
  # without zstd support
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_INCLUDES ${LibSolv_INCLUDE_DIRS} ${CMAKE_REQUIRED_INCLUDES})
  CHECK_SYMBOL_EXISTS( LIBSOLVEXT_FEATURE_ZLIB_COMPRESSION   "solv/solvversion.h" SOLV_NEED_ZLIB   )
  CHECK_SYMBOL_EXISTS( LIBSOLVEXT_FEATURE_LZMA_COMPRESSION   "solv/solvversion.h" SOLV_NEED_LZMA   )
  CHECK_SYMBOL_EXISTS( LIBSOLVEXT_FEATURE_BZIP2_COMPRESSION  "solv/solvversion.h" SOLV_NEED_BZIP2  )
  CHECK_SYMBOL_EXISTS( LIBSOLVEXT_FEATURE_ZSTD_COMPRESSION   "solv/solvversion.h" SOLV_NEED_ZSTD   )
  cmake_pop_check_state()

  # no check for system zchunk, default is usually to use the libsolv internal libs
  #CHECK_SYMBOL_EXISTS( LIBSOLVEXT_FEATURE_ZCHUNK_COMPRESSION "solv/solvversion.h" SOLV_NEED_ZCHUNK )
  #if ( SOLV_NEED_ZCHUNK )
  #  FIND_PACKAGE(PkgConfig REQUIRED)
  #  PKG_CHECK_MODULES(ZCHUNK zck REQUIRED)
  #  set(LibSolv_LIBRARIES ${LibSolv_LIBRARIES} ${ZCHUNK_LIBRARIES})
  #endif ()

  if ( SOLV_NEED_ZLIB )
    FIND_PACKAGE (ZLIB REQUIRED)
    SET(LibSolv_LIBRARIES ${LibSolv_LIBRARIES} ${ZLIB_LIBRARY})
  endif ()
  if ( SOLV_NEED_LZMA )
    FIND_LIBRARY(LZMA_LIBRARY NAMES lzma liblzma REQUIRED)
    SET(LibSolv_LIBRARIES ${LibSolv_LIBRARIES} ${LZMA_LIBRARY})
  endif ()
  if ( SOLV_NEED_BZIP2 )
    FIND_PACKAGE (BZip2 REQUIRED)
    SET(LibSolv_LIBRARIES ${LibSolv_LIBRARIES} ${BZIP2_LIBRARIES})
  endif ()
  if ( SOLV_NEED_ZSTD )
    FIND_LIBRARY (ZSTD_LIBRARY NAMES zstd REQUIRED)
    SET(LibSolv_LIBRARIES ${LibSolv_LIBRARIES} ${ZSTD_LIBRARY})
  endif ()
endif( ZYPP_STACK_BUILD )

target_include_directories( zypp_initial_compiler_flags INTERFACE ${LibSolv_INCLUDE_DIRS} )

pkg_search_module(GPGME gpgme>=1.8.0)
IF ( GPGME_FOUND )
  MESSAGE(STATUS "Found gpgme ${GPGME_VERSION}" )
  target_include_directories( zypp_initial_compiler_flags INTERFACE ${GPGME_INCLUDE_DIRS} )
  target_link_directories( zypp_initial_compiler_flags INTERFACE ${GPGME_LIBRARY_DIRS} )
ELSE()
  FIND_PACKAGE(Gpgme REQUIRED)
  IF ( NOT GPGME_PTHREAD_FOUND )
    MESSAGE( FATAL_ERROR " gpgme not found" )
  ELSE()
    target_include_directories( zypp_initial_compiler_flags INTERFACE ${GPGME_INCLUDES} )
    target_link_directories( zypp_initial_compiler_flags INTERFACE ${GPGME_LIBRARY_DIR} )
    SET(GPGME_LIBRARIES ${GPGME_PTHREAD_LIBRARIES})
  ENDIF()
ENDIF()

FIND_PACKAGE(OpenSSL REQUIRED)

FIND_PACKAGE(Udev)
IF ( NOT UDEV_FOUND )
  MESSAGE(WARNING "No udev found. CD device detection will be poor")
ELSE ( NOT UDEV_FOUND )
  target_compile_definitions( zypp_initial_compiler_flags INTERFACE HAVE_UDEV)
ENDIF ( NOT UDEV_FOUND )

IF( DISABLE_LIBPROXY )
  MESSAGE( STATUS "libproxy support disabled" )
ELSE( DISABLE_LIBPROXY )
  target_compile_definitions( zypp_initial_compiler_flags INTERFACE WITH_LIBPROXY_SUPPORT)
ENDIF( DISABLE_LIBPROXY )

FIND_PROGRAM( DOXYGEN doxygen )
IF ( NOT DOXYGEN )
  IF ( DISABLE_AUTODOCS )
    MESSAGE( STATUS "doxygen is not available. Can't build the documentation." )
  ELSE ( DISABLE_AUTODOCS )
    MESSAGE( FATAL_ERROR "doxygen not found: install doxygen to build the documentation!" )
  ENDIF ( DISABLE_AUTODOCS )
ELSE ( NOT DOXYGEN )
  MESSAGE( STATUS "doxygen found: ${DOXYGEN}" )
ENDIF ( NOT DOXYGEN )

if (ENABLE_CLANG_TIDY)
  FIND_PROGRAM( CLANG-TIDY clang-tidy )
  if ( CLANG-TIDY )
    message( "Enabling clang-tidy checks")
    set( ZYPP_CXX_CLANG_TIDY ${CLANG-TIDY} )
  else ( CLANG-TIDY )
    message(WARNING "clang-tidy not found.")
  endif( CLANG-TIDY )
endif(ENABLE_CLANG_TIDY)

if ( ENABLE_CPPCHECK )
  FIND_PROGRAM( CPPCHECK cppcheck )
  if ( CPPCHECK )
    message( "Enabling cppcheck")
    set(ZYPP_CXX_CPPCHECK ${CPPCHECK};--std=c++17;--suppress=rethrowNoCurrentException)
  else ( CPPCHECK )
    message(WARNING "cppcheck not found.")
  endif( CPPCHECK )
endif(ENABLE_CPPCHECK)

# https://bugzilla.gnome.org/show_bug.cgi?id=784550
IF (ENABLE_ZCHUNK_COMPRESSION)
  MESSAGE("Building with zchunk support enabled.")
  PKG_CHECK_MODULES (ZCHUNK zck REQUIRED)
  message("ZCHUNK FLAGS ${ZCHUNK_CFLAGS} ")
  target_compile_options( zypp_initial_compiler_flags INTERFACE ${ZCHUNK_CFLAGS}  )
  target_link_options( zypp_initial_compiler_flags INTERFACE ${ZCHUNK_CFLAGS}  )
  target_compile_definitions( zypp_initial_compiler_flags INTERFACE ENABLE_ZCHUNK_COMPRESSION=1 )
ENDIF(ENABLE_ZCHUNK_COMPRESSION)

IF(ENABLE_SIGC_BLOCK_WORKAROUND)
  message("Building with sigcpp block workaround")
  target_compile_definitions( zypp_initial_compiler_flags INTERFACE LIBZYPP_USE_SIGC_BLOCK_WORKAROUND=1)
ENDIF(ENABLE_SIGC_BLOCK_WORKAROUND)

pkg_check_modules ( SIGCPP REQUIRED sigc++-2.0 )
target_include_directories( zypp_initial_compiler_flags INTERFACE ${SIGCPP_INCLUDE_DIRS} )

pkg_check_modules ( LIBGLIB REQUIRED glib-2.0 )
target_include_directories( zypp_initial_compiler_flags INTERFACE ${LIBGLIB_INCLUDE_DIRS} )

#usage requirements for all libs
add_library( zypp_initial_lib_compiler_flags INTERFACE)
target_compile_options( zypp_initial_lib_compiler_flags INTERFACE "-fPIC" "-rdynamic" "-Wl,--no-allow-shlib-undefined"  "-Wl,--no-undefined")
target_link_options   ( zypp_initial_lib_compiler_flags INTERFACE "-fPIC" "-rdynamic" "-Wl,--no-allow-shlib-undefined"  "-Wl,--no-undefined" )

#usage requirements for all executables
add_library( zypp_initial_exe_compiler_flags INTERFACE)
target_compile_options( zypp_initial_exe_compiler_flags INTERFACE "-fpie" "-pie" "-Wl,--no-undefined" )
target_link_options   ( zypp_initial_exe_compiler_flags INTERFACE "-fpie" "-pie" "-Wl,--no-undefined" )

INCLUDE( zypp/VERSION.cmake )

MATH( EXPR LIBZYPP_NUMVERSION "${LIBZYPP_MAJOR} * 10000 + ${LIBZYPP_MINOR} * 100 + ${LIBZYPP_PATCH}" )
MATH( EXPR LIBZYPP_CURRENT "${LIBZYPP_MAJOR} * 100 + ${LIBZYPP_MINOR}" )
MATH( EXPR LIBZYPP_AGE     "${LIBZYPP_MINOR} - ${LIBZYPP_COMPATMINOR}" )
# Libtool wanted current:patch:age
# But cmake is not libtool, it wants the verbatim suffix to libzypp.so
MATH( EXPR LIBZYPP_SO_FIRST  "${LIBZYPP_CURRENT}-${LIBZYPP_AGE}" )
SET( VERSION "${LIBZYPP_MAJOR}.${LIBZYPP_MINOR}.${LIBZYPP_PATCH}" )

INCLUDE(CTest)
ENABLE_TESTING()

# we add the top level projects conditionally. This way we can exclude them from
# each others source tarball. ( not needed with _multibuild? )

if (BUILD_LIBZYPP)
  add_subdirectory(zypp)
  MESSAGE(STATUS "ZyppConfig.cmake will be installed in ${LIB_INSTALL_DIR}/cmake/Zypp")
  INSTALL( FILES ${zypp-libs_SOURCE_DIR}/cmake/modules/ZyppConfig.cmake DESTINATION ${LIB_INSTALL_DIR}/cmake/Zypp )
  INSTALL( FILES ${zypp-libs_SOURCE_DIR}/cmake/modules/ZyppCommon.cmake DESTINATION ${LIB_INSTALL_DIR}/cmake/Zypp )
endif()

if (BUILD_LIBZYPPNG)
  OPTION (EXPORT_NG_API "Export experimental libzypp API" OFF)
  OPTION (INSTALL_NG_BINARIES "Installs the NG binaries, disabled for now since we are not actively using them." OFF)
  add_subdirectory(zyppng)
endif()

IF ( ENABLE_BUILD_TRANS )
  ADD_SUBDIRECTORY( po )
ELSE ( ENABLE_BUILD_TRANS )
  ADD_SUBDIRECTORY( po EXCLUDE_FROM_ALL )
ENDIF ( ENABLE_BUILD_TRANS )


