project(zyppng)

SET ( CMAKE_CXX_STANDARD 20 )
SET ( CMAKE_CXX_EXTENSIONS OFF )


SET ( zyppng_lib_SRCS )

# Default loggroup for all files
#SET_LOGGROUP( "zypp" ${zyppng_lib_SRCS} )

if ( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )
  message(NOTICE "Enabling static analysis: ${ZYPP_CXX_CLANG_TIDY} ${ZYPP_CXX_CPPCHECK}")

  if (ZYPP_CXX_CLANG_TIDY)
    set( CMAKE_CXX_CLANG_TIDY ${ZYPP_CXX_CLANG_TIDY} )
  endif(ZYPP_CXX_CLANG_TIDY)

  if (ZYPP_CXX_CPPCHECK)
    set(CMAKE_CXX_CPPCHECK ${ZYPP_CXX_CPPCHECK})
  endif(ZYPP_CXX_CPPCHECK)
endif( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )


# general flags used for all targets
add_library           ( zyppng_general_compiler_flags INTERFACE )
target_link_libraries ( zyppng_general_compiler_flags INTERFACE zypp_initial_compiler_flags )

#target_compile_options( zyppng_general_compiler_flags INTERFACE "$<$<CONFIG:DEBUG>:-fsanitize=address>" )
#target_link_options( zyppng_general_compiler_flags    INTERFACE "$<$<CONFIG:DEBUG>:-fsanitize=address>" )
# -fconcepts-diagnostics-depth is GCC-only; Clang rejects unknown -f flags as errors.
# Clang's concept diagnostics are superior by default and need no tuning flag.
target_compile_options( zyppng_general_compiler_flags INTERFACE
  "$<$<AND:$<CONFIG:DEBUG>,$<CXX_COMPILER_ID:GNU>>:-fconcepts-diagnostics-depth=4>" )

target_compile_options( zyppng_general_compiler_flags INTERFACE "-fvisibility=hidden" )
target_link_options   ( zyppng_general_compiler_flags INTERFACE "-fvisibility=hidden" )

# When building with Clang, prefer the version-matched lld (e.g. lld-19 for
# clang++-19) for faster incremental builds and native .pcm file support.
# Falls back to unversioned lld, then silently skips if neither is found.
if ( CMAKE_CXX_COMPILER_ID STREQUAL "Clang" )
  # Extract major version from e.g. "19.1.7" → "19"
  string( REGEX MATCH "^[0-9]+" ZYPPNG_CLANG_MAJOR "${CMAKE_CXX_COMPILER_VERSION}" )

  find_program( ZYPPNG_LLD_EXECUTABLE
    NAMES "ld.lld-${ZYPPNG_CLANG_MAJOR}" "ld.lld"
    DOC   "lld ELF linker for zyppng Clang builds"
  )

  if ( ZYPPNG_LLD_EXECUTABLE )
    message( STATUS "zyppng: Clang ${ZYPPNG_CLANG_MAJOR} — using linker: ${ZYPPNG_LLD_EXECUTABLE}" )
    target_link_options( zyppng_general_compiler_flags INTERFACE
      "-fuse-ld=${ZYPPNG_LLD_EXECUTABLE}" )
  else()
    message( STATUS "zyppng: Clang detected but no lld found — using system linker" )
  endif()
endif()

add_library( zyppng_lib_compiler_flags INTERFACE)
target_link_libraries ( zyppng_lib_compiler_flags INTERFACE zypp_initial_lib_compiler_flags )
target_link_libraries ( zyppng_lib_compiler_flags INTERFACE zyppng_general_compiler_flags )

add_library( zyppng_exe_compiler_flags INTERFACE)
target_link_libraries ( zyppng_exe_compiler_flags INTERFACE zypp_initial_exe_compiler_flags )
target_link_libraries ( zyppng_exe_compiler_flags INTERFACE zyppng_general_compiler_flags )

IF ( ENABLE_DEVEL_BUILD )
        MESSAGE( WARNING "ZyppNG devel build enabled, do not do this in production" )
        SET( ZYPPNG_RPM_BINARY "${zyppng_BINARY_DIR}/tools/zyppng-rpm/zyppng-rpm")
        SET( ZYPPNG_WORKER_PATH "${zyppng_BINARY_DIR}/tools/workers" )
        if ( ZYPP_STACK_BUILD )
          target_compile_definitions( zyppng_general_compiler_flags INTERFACE ZYPP_REPO2SOLV_PATH="${zypp-stack_BINARY_DIR}/libsolv/tools/repo2solv"   )
          target_compile_definitions( zyppng_general_compiler_flags INTERFACE ZYPP_RPMDB2SOLV_PATH="${zypp-stack_BINARY_DIR}/libsolv/tools/rpmdb2solv" )
        endif()
ELSE()
        SET( ZYPPNG_RPM_BINARY "${ZYPPNG_LIBEXEC_INSTALL_DIR}/zyppng-rpm")
        SET( ZYPPNG_WORKER_PATH "${ZYPPNG_LIBEXEC_INSTALL_DIR}/workers" )
ENDIF( ENABLE_DEVEL_BUILD )

message ( "Using zyppng-rpm from path: ${ZYPPNG_RPM_BINARY}" )
target_compile_definitions(zyppng_general_compiler_flags INTERFACE ZYPP_RPM_BINARY="${ZYPPNG_RPM_BINARY}"  )
target_compile_definitions(zyppng_general_compiler_flags INTERFACE ZYPP_WORKER_PATH="${ZYPPNG_WORKER_PATH}")

# SoT: zyppng/packaging/libzypp-glib-version.inc (also %include'd by the .spec).
include( ${CMAKE_CURRENT_SOURCE_DIR}/api/VERSION.cmake )

set( ZYPPNG_WORKER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/tools/workers )

#signal the code to enable zyppng features
add_definitions( -DZYPP_ENABLE_ASYNC )
add_definitions( -DZYPPNG=1 )

add_subdirectory( lib )
add_subdirectory( api )
add_subdirectory( tools )
add_subdirectory( tests )
