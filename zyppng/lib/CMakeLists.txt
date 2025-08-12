
SET( ZYPP_LOGIC_TARGET_PREFIX "zyppng" )
SET( ZYPP_LOGIC_INSTALL_CXX   false    )


add_definitions( -DZYPP_ENABLE_ASYNC )

add_subdirectory( zypp-core )
add_subdirectory( zypp-common )
add_subdirectory( zypp-media )
add_subdirectory( zypp-curl )
add_subdirectory( zypp )
