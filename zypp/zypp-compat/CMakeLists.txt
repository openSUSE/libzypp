SET ( zypp_core_compat_HEADERS
  zypp/APIConfig.h
  zypp/AutoDispose.h
  zypp/ByteArray.h
  zypp/ByteCount.h
  zypp/ContentType.h
  zypp/Date.h
  zypp/ExternalProgram.h
  zypp/Globals.h
  zypp/KeyManager.h
  zypp/KVMap.h
  zypp/OnMediaLocation.h
  zypp/Pathname.h
  zypp/ProgressData.h
  zypp/PublicKey.h
  zypp/TriBool.h
  zypp/Url.h
  zypp/UserData.h
)

INSTALL( FILES
  ${zypp_core_compat_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp
)

SET ( zypp_core_base_compat_HEADERS
  zypp/base/DefaultIntegral.h
  zypp/base/DrunkenBishop.h
  zypp/base/DtorReset.h
  zypp/base/Easy.h
  zypp/base/EnumClass.h
  zypp/base/Env.h
  zypp/base/Errno.h
  zypp/base/Exception.h
  zypp/base/ExternalDataSource.h
  zypp/base/Flags.h
  zypp/base/Function.h
  zypp/base/fXstream.h
  zypp/base/Gettext.h
  zypp/base/GzStream.h
  zypp/base/Hash.h
  zypp/base/InputStream.h
  zypp/base/IOStream.h
  zypp/base/IOTools.h
  zypp/base/Iterable.h
  zypp/base/Iterator.h
  zypp/base/LogControl.h
  zypp/base/Logger.h
  zypp/base/LogTools.h
  zypp/base/NonCopyable.h
  zypp/base/ProfilingFormater.h
  zypp/base/ProvideNumericId.h
  zypp/base/PtrTypes.h
  zypp/base/Regex.h
  zypp/base/SimpleStreambuf.h
  zypp/base/String.h
  zypp/base/StringV.h
  zypp/base/Sysconfig.h
  zypp/base/TypeTraits.h
  zypp/base/Unit.h
  zypp/base/UserRequestException.h
  zypp/base/WatchFile.h
  zypp/base/Xml.h
)

IF (ENABLE_ZCHUNK_COMPRESSION)
list( APPEND zypp_core_base_compat_HEADERS
  zypp/base/ZckStream.h
)
ENDIF(ENABLE_ZCHUNK_COMPRESSION)

INSTALL( FILES
  ${zypp_core_base_compat_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/base
)

SET ( zypp_core_url_compat_HEADERS
  zypp/url/UrlException.h
  zypp/url/UrlUtils.h
  zypp/url/UrlBase.h
)

INSTALL( FILES
  ${zypp_core_url_compat_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/url
)


SET ( zypp_media_compat_HEADERS
  zypp/media/CredentialFileReader.h
  zypp/media/CredentialManager.h
  zypp/media/MediaException.h
  zypp/media/MediaBlockList.h
  zypp/media/MediaUserAuth.h
  zypp/media/MetaLinkParser.h
  zypp/media/ProxyInfo.h
  zypp/media/TransferSettings.h
  zypp/media/Mount.h
  zypp/media/CurlConfig.h
)

INSTALL( FILES
  ${zypp_media_compat_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/media
)

IF ( LIBPROXY_FOUND )
  SET(zypp_media_proxyinfo_libproxy_HEADERS zypp/media/proxyinfo/ProxyInfoLibproxy.h)
ENDIF( LIBPROXY_FOUND )

SET( zypp_media_compat_proxyinfo_HEADERS
  zypp/media/proxyinfo/ProxyInfoImpl.h
  zypp/media/proxyinfo/ProxyInfoSysconfig.h
  ${zypp_media_proxyinfo_libproxy_HEADERS}
  zypp/media/proxyinfo/ProxyInfos.h
)

INSTALL(  FILES
  ${zypp_media_compat_proxyinfo_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/media/proxyinfo
)

SET ( zypp_core_parser_xml_compat_HEADERS
  zypp/parser/xml/XmlEscape.h
)

INSTALL( FILES
  ${zypp_core_parser_xml_compat_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/parser/xml
)


SET ( zypp_core_parser_compat_HEADERS
  zypp/parser/IniDict.h
  zypp/parser/IniParser.h
  zypp/parser/ParseException.h
)

INSTALL( FILES
  ${zypp_core_parser_compat_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/parser
)
