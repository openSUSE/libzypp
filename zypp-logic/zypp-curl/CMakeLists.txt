# Collect all files that may contain translatable texts
FILE( GLOB_RECURSE POT_FILE_DEPENDS RELATIVE ${LIBZYPP_SOURCE_DIR} "*.h" "*.cc" )
SET( POT_FILE_DEPENDS_ZYPP_CURL ${POT_FILE_DEPENDS} PARENT_SCOPE )

INCLUDE_DIRECTORIES ( ${LIBZYPP_SOURCE_DIR} )

IF (ENABLE_ZCHUNK_COMPRESSION)
  INCLUDE_DIRECTORIES( ${ZCHUNK_INCLUDEDIR} )
ENDIF(ENABLE_ZCHUNK_COMPRESSION)

ADD_DEFINITIONS( -DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale" -DTEXTDOMAIN="zypp" -DZYPP_DLL )

SET( zypp_curl_HEADERS
  CurlConfig
  curlconfig.h
  ProxyInfo
  proxyinfo.h
  TransferSettings
  transfersettings.h
)

SET( zypp_curl_private_HEADERS
  private/curlhelper_p.h
)

SET( zypp_curl_SRCS
  curlconfig.cc
  proxyinfo.cc
  curlhelper.cc
  transfersettings.cc
)

if( ZYPP_LOGIC_INSTALL_CXX )
  INSTALL(  FILES ${zypp_curl_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-curl" )
endif()

SET( zypp_curl_auth_HEADERS
  auth/CurlAuthData
  auth/curlauthdata.h
)

SET( zypp_curl_auth_private_HEADERS
)

SET( zypp_curl_auth_SRCS
  auth/curlauthdata.cc
)

if( ZYPP_LOGIC_INSTALL_CXX )
  INSTALL(  FILES ${zypp_curl_auth_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-curl/auth" )
endif()

IF( NOT DISABLE_LIBPROXY )
  SET(zypp_curl_proxyinfo_libproxy_SRCS proxyinfo/proxyinfolibproxy.cc)
  SET(zypp_curl_proxyinfo_libproxy_HEADERS proxyinfo/ProxyInfoLibproxy proxyinfo/proxyinfolibproxy.h)
ENDIF( )

SET( zypp_curl_proxyinfo_SRCS
  proxyinfo/proxyinfosysconfig.cc
  ${zypp_curl_proxyinfo_libproxy_SRCS}
)

SET( zypp_curl_proxyinfo_HEADERS
  proxyinfo/proxyinfoimpl.h
  proxyinfo/ProxyInfoSysconfig
  proxyinfo/proxyinfosysconfig.h
  ${zypp_curl_proxyinfo_libproxy_HEADERS}
  proxyinfo/proxyinfos.h
)

if( ZYPP_LOGIC_INSTALL_CXX )
  INSTALL(  FILES ${zypp_curl_proxyinfo_HEADERS} DESTINATION ${INCLUDE_INSTALL_DIR}/zypp-curl/proxyinfo )
endif()

SET( zypp_curl_ng_network_SRCS
  ng/network/curlmultiparthandler.cc
  ng/network/networkrequestdispatcher.cc
  ng/network/networkrequesterror.cc
  ng/network/request.cc
)

SET( zypp_curl_ng_network_HEADERS
  ng/network/curlmultiparthandler.h
  ng/network/AuthData
  ng/network/authdata.h
  ng/network/NetworkRequestDispatcher
  ng/network/networkrequestdispatcher.h
  ng/network/NetworkRequestError
  ng/network/networkrequesterror.h
  ng/network/rangedesc.h
  ng/network/Request
  ng/network/request.h
  ng/network/TransferSettings
  ng/network/transfersettings.h
)

if (ENABLE_ZCHUNK_COMPRESSION)
  list( APPEND zypp_curl_ng_network_HEADERS
    ng/network/zckhelper.h
  )
  list( APPEND zypp_curl_ng_network_SRCS
    ng/network/zckhelper.cc
  )
endif()

SET( zypp_curl_ng_network_private_HEADERS
  ng/network/private/mediadebug_p.h
  ng/network/private/networkrequestdispatcher_p.h
  ng/network/private/networkrequesterror_p.h
  ng/network/private/request_p.h
)

SET( zypp_curl_parser_HEADERS
  parser/MediaBlockList
  parser/mediablocklist.h
  parser/metadatahelper.h
  parser/MetaLinkParser
  parser/metalinkparser.h
  parser/ZsyncParser
  parser/zsyncparser.h
)

SET( zypp_curl_parser_private_HEADERS
)

SET( zypp_curl_parser_SRCS
  parser/mediablocklist.cc
  parser/metadatahelper.cc
  parser/metalinkparser.cc
  parser/zsyncparser.cc
)

if( ZYPP_LOGIC_INSTALL_CXX )
  INSTALL(  FILES ${zypp_curl_parser_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-curl/parser" )
endif()


SET( zypp_curl_lib_SRCS
    ${zypp_curl_SRCS}
    ${zypp_curl_auth_SRCS}
    ${zypp_curl_ng_network_SRCS}
    ${zypp_curl_parser_SRCS}
    ${zypp_curl_proxyinfo_SRCS}
    ${zypp_curl_ng_network_private_SRCS}
)
SET( zypp_curl_lib_HEADERS
    ${zypp_curl_private_HEADERS} ${zypp_curl_HEADERS}
    ${zypp_curl_auth_private_HEADERS} ${zypp_curl_auth_HEADERS}
    ${zypp_curl_ng_network_HEADERS} ${zypp_curl_ng_network_private_HEADERS}
    ${zypp_curl_parser_private_HEADERS} ${zypp_curl_parser_HEADERS}
    ${zypp_curl_proxyinfo_HEADERS}
)

# Default loggroup for all files
SET_LOGGROUP( "zypp-curl" $${zypp_curl_lib_SRCS} )

if ( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )
  if (ZYPP_CXX_CLANG_TIDY)
    set( CMAKE_CXX_CLANG_TIDY ${ZYPP_CXX_CLANG_TIDY} )
  endif(ZYPP_CXX_CLANG_TIDY)

  if (ZYPP_CXX_CPPCHECK)
    set(CMAKE_CXX_CPPCHECK ${ZYPP_CXX_CPPCHECK})
  endif(ZYPP_CXX_CPPCHECK)
endif( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

ADD_LIBRARY( ${ZYPP_LOGIC_TARGET_PREFIX}-curl STATIC ${zypp_curl_lib_SRCS} ${zypp_curl_lib_HEADERS}
  ng/network/curlmultiparthandler.h ng/network/curlmultiparthandler.cc )

target_link_libraries( ${ZYPP_LOGIC_TARGET_PREFIX}-curl PRIVATE zypp_lib_compiler_flags )
target_link_libraries( ${ZYPP_LOGIC_TARGET_PREFIX}-curl INTERFACE ${CURL_LIBRARIES} )
target_link_libraries( ${ZYPP_LOGIC_TARGET_PREFIX}-curl INTERFACE ${LIBXML2_LIBRARIES} )

IF( NOT DISABLE_LIBPROXY )
  # required for dlload and dlclose
  target_link_libraries( ${ZYPP_LOGIC_TARGET_PREFIX}-curl  INTERFACE ${CMAKE_DL_LIBS} )
ENDIF()

get_filename_component(PARENT_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
get_filename_component(PARENT_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR} DIRECTORY)
target_include_directories( ${ZYPP_LOGIC_TARGET_PREFIX}-curl  INTERFACE
  $<BUILD_INTERFACE:${PARENT_SRC_DIR}>
  $<BUILD_INTERFACE:${PARENT_BIN_DIR}>
)
