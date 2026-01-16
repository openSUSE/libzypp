PROJECT(zyppng-cxx)

OPTION( LIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT "Builtin default for zypp.conf:download.use_deltarpm" OFF )
OPTION( LIBZYPP_CONFIG_USE_CLASSIC_RPMTRANS_BY_DEFAULT "Use the CLASSIC_RPMTRANS install backend rather than SINGLE_RPMTRANS" OFF )
OPTION( LIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT "By default use the old CURL backend for downloads" OFF )
OPTION( LIBZYPP_CONFIG_USE_SERIAL_PACKAGE_DOWNLOAD_BY_DEFAULT "By default don't download packages in parallel" OFF )

SET( ZYPP_LOGIC_TARGET_PREFIX "zyppng" )
SET( ZYPP_LOGIC_INSTALL_CXX   false    )

add_subdirectory( shared )
add_subdirectory( zypp-core )
add_subdirectory( zypp-common )
add_subdirectory( zypp-media )
add_subdirectory( zypp-curl )
#add_subdirectory( zypp )


set ( target "zyppng-allsym" )
set ( _link_mode INTERFACE )
ADD_LIBRARY( ${target} STATIC zypp/dummy.cc )

SET_TARGET_PROPERTIES ( ${target} PROPERTIES VERSION "${LIBZYPP_VERSION_INFO}" )
SET_TARGET_PROPERTIES ( ${target} PROPERTIES SOVERSION "${LIBZYPP_SOVERSION_INFO}" )

target_link_libraries( ${target} PRIVATE zyppng_lib_compiler_flags )
#target_link_libraries( ${target} PRIVATE zyppng-objlib )
target_link_libraries( ${target} PRIVATE zyppng-commit-proto-obj )

# CAUTION, link those libs always first and always in that order,
# otherwise you get symbol lookup errors when linking against libzypp
target_link_libraries( ${target} ${_link_mode} zyppng-common )
target_link_libraries( ${target} ${_link_mode} zyppng-curl )
target_link_libraries( ${target} ${_link_mode} zyppng-media )


# CAUTION, link zypp-tui.a at last, to prevent undefined references
# in case tui functions are used in libzypp.
#target_link_libraries( ${target} ${_link_mode} zypp-tui )

target_link_libraries( ${target} ${_link_mode} zyppng-core )

target_link_libraries( ${target} ${_link_mode} ${LibSolv_LIBRARIES} )
target_link_libraries( ${target} ${_link_mode} ${RPM_LIBRARY} ${RPMIO_LIBRARY} )
target_link_libraries( ${target} ${_link_mode} ${GETTEXT_LIBRARIES} )
target_link_libraries( ${target} ${_link_mode} ${Boost_THREAD_LIBRARY})
target_link_libraries( ${target} ${_link_mode} ${YAML_CPP_LIBRARIES})

IF ( UDEV_FOUND )
  target_link_libraries( ${target} ${_link_mode} ${UDEV_LIBRARY} )
ELSE ( UDEV_FOUND )
  IF ( HAL_FOUND )
    target_link_libraries( ${target} ${_link_mode} ${HAL_LIBRARY} ${HAL_STORAGE_LIBRARY} ${DBUS_LIBRARY} )
  ENDIF ( HAL_FOUND )
ENDIF ( UDEV_FOUND )
