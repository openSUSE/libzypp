include(${zypp-libs_SOURCE_DIR}/zypp-logic/zypp-curl/zypp-curl.cmake)
zypp_add_curl_target( TARGETNAME "zypp-curl" FLAGS zypp_lib_compiler_flags INSTALL_HEADERS )
