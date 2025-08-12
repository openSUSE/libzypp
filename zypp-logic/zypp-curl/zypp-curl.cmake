include(${CMAKE_CURRENT_LIST_DIR}/../logic-helper.cmake)


function( zypp_add_curl_target )

  set(options INSTALL_HEADERS )
  set(oneValueArgs TARGETNAME)
  set(multiValueArgs HEADERS SOURCES )

  cmake_parse_arguments(PARSE_ARGV 0 arg "${options}" "${oneValueArgs}" "${multiValueArgs}")


IF (ENABLE_ZCHUNK_COMPRESSION)
  INCLUDE_DIRECTORIES( ${ZCHUNK_INCLUDEDIR} )
ENDIF(ENABLE_ZCHUNK_COMPRESSION)

ADD_DEFINITIONS( -DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale" -DTEXTDOMAIN="zypp" -DZYPP_DLL )

zypp_add_sources( zypp_curl_HEADERS
  CurlConfig
  curlconfig.h
  ProxyInfo
  proxyinfo.h
  TransferSettings
  transfersettings.h
)

zypp_add_sources( zypp_curl_private_HEADERS
  private/curlhelper_p.h
)

zypp_add_sources( zypp_curl_SRCS
  curlconfig.cc
  proxyinfo.cc
  curlhelper.cc
  transfersettings.cc
)

if( arg_INSTALL_HEADERS )
  INSTALL(  FILES ${zypp_curl_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-curl" )
endif()

zypp_add_sources( zypp_curl_auth_HEADERS
  auth/CurlAuthData
  auth/curlauthdata.h
)

zypp_add_sources( zypp_curl_auth_private_HEADERS
)

zypp_add_sources( zypp_curl_auth_SRCS
  auth/curlauthdata.cc
)

if( arg_INSTALL_HEADERS )
  INSTALL(  FILES ${zypp_curl_auth_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-curl/auth" )
endif()

IF( NOT DISABLE_LIBPROXY )
  zypp_add_sources(zypp_curl_proxyinfo_SRCS proxyinfo/proxyinfolibproxy.cc)
  zypp_add_sources(zypp_curl_proxyinfo_HEADERS proxyinfo/ProxyInfoLibproxy proxyinfo/proxyinfolibproxy.h)
ENDIF( )

zypp_add_sources( zypp_curl_proxyinfo_SRCS
  proxyinfo/proxyinfosysconfig.cc
)

zypp_add_sources( zypp_curl_proxyinfo_HEADERS
  proxyinfo/proxyinfoimpl.h
  proxyinfo/ProxyInfoSysconfig
  proxyinfo/proxyinfosysconfig.h
  proxyinfo/proxyinfos.h
)

if( arg_INSTALL_HEADERS )
  INSTALL(  FILES ${zypp_curl_proxyinfo_HEADERS} DESTINATION ${INCLUDE_INSTALL_DIR}/zypp-curl/proxyinfo )
endif()

zypp_add_sources( zypp_curl_ng_network_SRCS
  ng/network/curlmultiparthandler.cc
  ng/network/networkrequestdispatcher.cc
  ng/network/networkrequesterror.cc
  ng/network/request.cc
)

zypp_add_sources( zypp_curl_ng_network_HEADERS
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
  zypp_add_sources( zypp_curl_ng_network_HEADERS
    ng/network/zckhelper.h
  )
  zypp_add_sources( zypp_curl_ng_network_SRCS
    ng/network/zckhelper.cc
  )
endif()

zypp_add_sources( zypp_curl_ng_network_private_HEADERS
  ng/network/private/mediadebug_p.h
  ng/network/private/networkrequestdispatcher_p.h
  ng/network/private/networkrequesterror_p.h
  ng/network/private/request_p.h
)

zypp_add_sources( zypp_curl_parser_HEADERS
  parser/MediaBlockList
  parser/mediablocklist.h
  parser/metadatahelper.h
  parser/MetaLinkParser
  parser/metalinkparser.h
  parser/ZsyncParser
  parser/zsyncparser.h
)

zypp_add_sources( zypp_curl_parser_private_HEADERS
)

zypp_add_sources( zypp_curl_parser_SRCS
  parser/mediablocklist.cc
  parser/metadatahelper.cc
  parser/metalinkparser.cc
  parser/zsyncparser.cc
)

if( arg_INSTALL_HEADERS )
  INSTALL(  FILES ${zypp_curl_parser_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-curl/parser" )
endif()


SET( zypp_curl_lib_SRCS
    ${arg_SOURCES}
    ${zypp_curl_SRCS}
    ${zypp_curl_auth_SRCS}
    ${zypp_curl_ng_network_SRCS}
    ${zypp_curl_parser_SRCS}
    ${zypp_curl_proxyinfo_SRCS}
    ${zypp_curl_ng_network_private_SRCS}
)
SET( zypp_curl_lib_HEADERS
    ${arg_HEADERS}
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

ADD_LIBRARY( ${arg_TARGETNAME} STATIC ${zypp_curl_lib_SRCS} ${zypp_curl_lib_HEADERS} )

target_link_libraries( ${arg_TARGETNAME} PRIVATE zypp_lib_compiler_flags )
target_link_libraries( ${arg_TARGETNAME} INTERFACE ${CURL_LIBRARIES} )
target_link_libraries( ${arg_TARGETNAME} INTERFACE ${LIBXML2_LIBRARIES} )

IF( NOT DISABLE_LIBPROXY )
  # required for dlload and dlclose
  target_link_libraries( ${arg_TARGETNAME}  INTERFACE ${CMAKE_DL_LIBS} )
ENDIF()

zypp_logic_setup_includes()

endfunction()
