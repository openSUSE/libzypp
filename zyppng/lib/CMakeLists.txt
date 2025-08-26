
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
