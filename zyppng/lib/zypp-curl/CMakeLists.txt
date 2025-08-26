include(${zypp-libs_SOURCE_DIR}/zypp-logic/zypp-curl/zypp-curl.cmake)
zypp_add_curl_target( TARGETNAME "zyppng-curl" FLAGS zyppng_lib_compiler_flags )
