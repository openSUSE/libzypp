include(${zypp-libs_SOURCE_DIR}/zypp-logic/zypp-media/zypp-media.cmake)
zypp_add_media_target( TARGETNAME "zypp-media" FLAGS zypp_lib_compiler_flags INSTALL_HEADERS)
