PROJECT(LIBZYPP)
SET( PACKAGE "libzypp" )

#--------------------------------------------------------------------------------
# Builtin defaults for APIConfig.h
# Legacy default behavior which might be enabled for legacy codestreams.
# Some features can nevertheless be enabled via zypp.conf or environment variables
# but may not be fully supported by all legacy applications.
OPTION( LIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT "Builtin default for zypp.conf:download.use_deltarpm" OFF )
OPTION( LIBZYPP_CONFIG_USE_CLASSIC_RPMTRANS_BY_DEFAULT "Use the CLASSIC_RPMTRANS install backend rather than SINGLE_RPMTRANS" OFF )
OPTION( LIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT "By default use the old CURL backend for downloads" OFF )
OPTION( LIBZYPP_CONFIG_USE_SERIAL_PACKAGE_DOWNLOAD_BY_DEFAULT "By default don't download packages in parallel" OFF )

#--------------------------------------------------------------------------------

# general flags used for all targets
add_library( zypp_general_compiler_flags INTERFACE )
target_link_libraries ( zypp_general_compiler_flags INTERFACE zypp_initial_compiler_flags )

IF ( ENABLE_DEVEL_BUILD )
        MESSAGE( WARNING "Zypp devel build enabled, do not do this in production" )
        SET( ZYPP_RPM_BINARY "${LIBZYPP_BINARY_DIR}/tools/zypp-rpm/zypp-rpm")
        SET( ZYPP_WORKER_PATH "${LIBZYPP_BINARY_DIR}/tools/workers" )
        if ( ZYPP_STACK_BUILD )
          target_compile_definitions( zypp_general_compiler_flags INTERFACE ZYPP_REPO2SOLV_PATH="${zypp-stack_BINARY_DIR}/libsolv/tools/repo2solv"   )
          target_compile_definitions( zypp_general_compiler_flags INTERFACE ZYPP_RPMDB2SOLV_PATH="${zypp-stack_BINARY_DIR}/libsolv/tools/rpmdb2solv" )
        endif()
ELSE()
        SET( ZYPP_RPM_BINARY "${ZYPP_LIBEXEC_INSTALL_DIR}/zypp-rpm")
        SET( ZYPP_WORKER_PATH "${ZYPP_LIBEXEC_INSTALL_DIR}/workers" )
ENDIF( ENABLE_DEVEL_BUILD )

message ( "Using zypp-rpm from path: ${ZYPP_RPM_BINARY}" )
target_compile_definitions(zypp_general_compiler_flags INTERFACE ZYPP_RPM_BINARY="${ZYPP_RPM_BINARY}"  )
target_compile_definitions(zypp_general_compiler_flags INTERFACE ZYPP_WORKER_PATH="${ZYPP_WORKER_PATH}")

#--------------------------------------------------------------------------------

SET (CMAKE_CXX_STANDARD ${ZYPPCOMMON_CXX_STANDARD})
SET (CMAKE_CXX_EXTENSIONS OFF)

IF ( ENABLE_VISIBILITY_HIDDEN )
  target_compile_options( zypp_general_compiler_flags INTERFACE "-fvisibility=hidden" )
  target_link_options( zypp_general_compiler_flags INTERFACE "-fvisibility=hidden" )
ENDIF( ENABLE_VISIBILITY_HIDDEN )

add_library( zypp_lib_compiler_flags INTERFACE)
target_link_libraries ( zypp_lib_compiler_flags INTERFACE zypp_initial_lib_compiler_flags )
target_link_libraries ( zypp_lib_compiler_flags INTERFACE zypp_general_compiler_flags )

add_library( zypp_exe_compiler_flags INTERFACE)
target_link_libraries ( zypp_exe_compiler_flags INTERFACE zypp_initial_exe_compiler_flags )
target_link_libraries ( zypp_exe_compiler_flags INTERFACE zypp_general_compiler_flags )

SET( LIBZYPP_VERSION_INFO "${LIBZYPP_SO_FIRST}.${LIBZYPP_AGE}.${LIBZYPP_PATCH}" )
SET( LIBZYPP_SOVERSION_INFO "${LIBZYPP_SO_FIRST}" )

GENERATE_PACKAGING(${PACKAGE} ${VERSION})

MESSAGE(STATUS "soname: ${LIBZYPP_VERSION_INFO}")
MESSAGE(STATUS "version: ${VERSION}")

MESSAGE(STATUS "Writing pkg-config file...")
CONFIGURE_FILE(${LIBZYPP_SOURCE_DIR}/libzypp.pc.cmake ${LIBZYPP_BINARY_DIR}/libzypp.pc @ONLY)
INSTALL( FILES ${LIBZYPP_BINARY_DIR}/libzypp.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig )

####################################################################
# config templates
# (don't forget to mention them in the .spec file)
####################################################################

MESSAGE(STATUS "zypp.conf will be installed in ${ZYPPCONFDIR}/zypp")
INSTALL( FILES ${LIBZYPP_SOURCE_DIR}/zypp.conf DESTINATION ${ZYPPCONFDIR}/zypp )
IF ( KEEP_LEGACY_ZYPPCONF )
MESSAGE(STATUS "LEGACY zypp.conf will be installed in ${SYSCONFDIR}/zypp")
INSTALL( FILES ${LIBZYPP_SOURCE_DIR}/zypp.conf.LEGACY DESTINATION ${SYSCONFDIR}/zypp/ RENAME zypp.conf )
ELSE()
INSTALL( FILES ${LIBZYPP_SOURCE_DIR}/zypp.conf.README DESTINATION ${SYSCONFDIR}/zypp )
ENDIF()

MESSAGE(STATUS "needreboot will be installed in ${SYSCONFDIR}/zypp/")
INSTALL( FILES ${LIBZYPP_SOURCE_DIR}/needreboot DESTINATION ${SYSCONFDIR}/zypp )

#install systemCheck
MESSAGE(STATUS "systemCheck will be installed in ${SYSCONFDIR}/zypp")
INSTALL( FILES ${LIBZYPP_SOURCE_DIR}/systemCheck DESTINATION ${SYSCONFDIR}/zypp )

# logrotate config file
INSTALL( FILES ${LIBZYPP_SOURCE_DIR}/zypp-history.lr DESTINATION ${SYSCONFDIR}/logrotate.d )

IF ( LIBZYPP_CONFIG_USE_CLASSIC_RPMTRANS_BY_DEFAULT )
  MESSAGE( STATUS "Default install backend: CLASSIC_RPMTRANS" )
ELSE()
  MESSAGE( STATUS "Default install backend: SINGLE_RPMTRANS" )
ENDIF()
IF ( LIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT )
  MESSAGE( STATUS "Legacy enabled: LIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT" )
ENDIF()
IF ( LIBZYPP_CONFIG_USE_SERIAL_PACKAGE_DOWNLOAD_BY_DEFAULT )
  MESSAGE( STATUS "Legacy enabled: LIBZYPP_CONFIG_USE_SERIAL_PACKAGE_DOWNLOAD_BY_DEFAULT" )
ENDIF()

####################################################################
# SUBDIRECTORIES                                                   #
####################################################################
SET( ZYPP_LOGIC_TARGET_PREFIX "zypp" )
SET( ZYPP_LOGIC_INSTALL_CXX true )

add_subdirectory( zypp-core )
add_subdirectory( zypp-tui )
add_subdirectory( zypp-compat )
add_subdirectory( zypp-common )
add_subdirectory( zypp-media )
add_subdirectory( zypp-curl )
add_subdirectory( zypp )
add_subdirectory( tools )
add_subdirectory( shared )
add_subdirectory( doc )

IF ( ENABLE_BUILD_TESTS )
  ADD_SUBDIRECTORY( tests )
ELSE ( ENABLE_BUILD_TESTS )
  ADD_SUBDIRECTORY( tests EXCLUDE_FROM_ALL )
ENDIF ( ENABLE_BUILD_TESTS )
