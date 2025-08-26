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

target_compile_options( zyppng_general_compiler_flags INTERFACE "-fvisibility=hidden" )
target_link_options   ( zyppng_general_compiler_flags INTERFACE "-fvisibility=hidden" )

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

#signal the code to enable zyppng features
add_definitions( -DZYPP_ENABLE_ASYNC )

add_subdirectory( lib )
add_subdirectory( api )
add_subdirectory( tools )
add_subdirectory( tests )

# this part does nothing yet, only when switching away from using a map file to
# a approach where symbols are exported explicitly it would make a difference
# ZYPPNG symbols need to be added to the map file to be exported
# IF( EXPORT_NG_API )
# ADD_DEFINITIONS(-DEXPORT_EXPERIMENTAL_API)
# INCLUDE(GenerateExportHeader)
# message("Exporting experimental libzypp-ng API")
# GENERATE_EXPORT_HEADER(
#   zypp
#   BASE_NAME zypp-ng
#   PREFIX_NAME LIB
# )
# ENDIF ( EXPORT_NG_API )
