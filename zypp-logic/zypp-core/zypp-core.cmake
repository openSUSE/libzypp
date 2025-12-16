include(${CMAKE_CURRENT_LIST_DIR}/../logic-helper.cmake)


function( zypp_add_core_target )

  set(options INSTALL_HEADERS )
  set(oneValueArgs TARGETNAME FLAGS )
  set(multiValueArgs HEADERS SOURCES )

  cmake_parse_arguments(PARSE_ARGV 0 arg "${options}" "${oneValueArgs}" "${multiValueArgs}")

ADD_DEFINITIONS( -DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale" -DTEXTDOMAIN="zypp" -DZYPP_DLL )

CONFIGURE_FILE ( ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/APIConfig.h.in APIConfig.h @ONLY )

zypp_add_sources( zypp_toplevel_headers
  AutoDispose.h
  ByteArray.h
  ByteCount.h
  CheckSum.h
  ContentType.h
  Date.h
  Digest.h
  ExternalProgram.h
  Globals.h
  KVMap
  kvmap.h
  ManagedFile.h
  MirroredOrigin.h
  onmedialocation.h
  OnMediaLocation
  Pathname.h
  TriBool.h
  Url.h
  UserData.h
)

zypp_add_sources( zypp_toplevel_SRCS
  ByteCount.cc
  CheckSum.cc
  Date.cc
  Digest.cc
  ExternalProgram.cc
  MirroredOrigin.cc
  onmedialocation.cc
  Pathname.cc
  ShutdownLock.cc
  Url.cc
)

if( ${arg_INSTALL_HEADERS} )
  INSTALL(  FILES ${zypp_toplevel_headers} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core" )
  INSTALL(  FILES ${CMAKE_CURRENT_BINARY_DIR}/APIConfig.h DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core" )
endif()

zypp_add_sources( zypp_base_HEADERS
  base/DefaultIntegral
  base/defaultintegral.h
  base/DtorReset
  base/dtorreset.h
  base/Easy.h
  base/EnumClass.h
  base/Env.h
  base/Errno.h
  base/Exception.h
  base/ExternalDataSource.h
  base/Function.h
  base/Flags.h
  base/filestreambuf.h
  base/FileStreamBuf
  base/fXstream
  base/fxstream.h
  base/Gettext.h
  base/GzStream
  base/gzstream.h
  base/Hash.h
  base/InputStream
  base/inputstream.h
  base/IOStream.h
  base/IOTools.h
  base/Iterable.h
  base/Iterator.h
  base/LogControl.h
  base/LogTools.h
  base/Logger.h
  base/NonCopyable.h
  base/ProfilingFormater.h
  base/ProvideNumericId
  base/providenumericid.h
  base/PtrTypes.h
  base/ReferenceCounted.h
  base/Regex.h
  base/SimpleStreambuf
  base/simplestreambuf.h
  base/String.h
  base/StringV.h
  base/TypeTraits.h
  base/Unit.h
  base/UserRequestException
  base/userrequestexception.h
  base/Xml.h
)

zypp_add_sources( zypp_base_SRCS
  base/CleanerThread.cc
  base/Exception.cc
  base/ExternalDataSource.cc
  base/filestreambuf.cc
  base/Gettext.cc
  base/gzstream.cc
  base/inputstream.cc
  base/IOStream.cc
  base/IOTools.cc
  base/LogControl.cc
  base/ProfilingFormater.cc
  base/ReferenceCounted.cc
  base/Regex.cc
  base/String.cc
  base/StringV.cc
  base/Unit.cc
  base/userrequestexception.cc
  base/Xml.cc
)

IF (ENABLE_ZCHUNK_COMPRESSION)

  zypp_add_sources( zypp_base_SRCS
    base/zckstream.cc
  )

  zypp_add_sources( zypp_base_HEADERS
    base/ZckStream
    base/zckstream.h
  )

ENDIF(ENABLE_ZCHUNK_COMPRESSION)

if( ${arg_INSTALL_HEADERS} )
  INSTALL(  FILES ${zypp_base_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/base" )
endif()


zypp_add_sources( zypp_fs_SRCS
  fs/PathInfo.cc
  fs/TmpPath.cc
)

zypp_add_sources( zypp_fs_HEADERS
  fs/PathInfo.h
  fs/TmpPath.h
  fs/WatchFile
  fs/watchfile.h
)

if( ${arg_INSTALL_HEADERS} )
  INSTALL(  FILES ${zypp_fs_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/fs" )
endif()


zypp_add_sources( zypp_rpc_SRCS
  rpc/PluginFrame.cc
  rpc/PluginFrameException.cc
)

zypp_add_sources( zypp_rpc_HEADERS
  rpc/PluginFrame.h
  rpc/PluginFrameException.h
)

if( ${arg_INSTALL_HEADERS} )
  INSTALL(  FILES ${zypp_rpc_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/rpc" )
endif()


zypp_add_sources( zypp_ui_SRCS
  ui/progressdata.cc
)

zypp_add_sources( zypp_ui_HEADERS
  ui/ProgressData
  ui/progressdata.h
)

if( ${arg_INSTALL_HEADERS} )
  INSTALL(  FILES ${zypp_ui_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/ui" )
endif()


zypp_add_sources( zypp_url_SRCS
  url/UrlUtils.cc
  url/UrlBase.cc
)

zypp_add_sources( zypp_url_HEADERS
  url/UrlBase.h
  url/UrlException.h
  url/UrlUtils.h
)

if( ${arg_INSTALL_HEADERS} )
  INSTALL(  FILES ${zypp_url_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/url" )
endif()

zypp_add_sources( zypp_parser_SRCS
  parser/iniparser.cc
  parser/inidict.cc
  parser/econfdict.cc
  parser/json.cc
  parser/json/JsonValue.cc
  parser/parseexception.cc
  parser/sysconfig.cc
)

zypp_add_sources ( zypp_parser_private_HEADERS
  parser/json/JsonBool.h
  parser/json/JsonNull.h
  parser/json/JsonNumber.h
  parser/json/JsonString.h
  parser/json/JsonValue.h
)

zypp_add_sources( zypp_parser_HEADERS
  parser/IniParser
  parser/iniparser.h
  parser/IniDict
  parser/inidict.h
  parser/EconfDict
  parser/econfdict.h
  parser/json.h
  parser/ParseException
  parser/parseexception.h
  parser/Sysconfig
  parser/sysconfig.h
)

if( ${arg_INSTALL_HEADERS} )
  INSTALL(  FILES ${zypp_parser_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/parser" )
endif()

zypp_add_sources( zypp_parser_xml_SRCS
  parser/xml/XmlEscape.cc
)

zypp_add_sources( zypp_parser_xml_HEADERS
  parser/xml/XmlEscape.h
)

if( ${arg_INSTALL_HEADERS} )
  INSTALL(  FILES ${zypp_parser_xml_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/parser/xml" )
endif()

zypp_add_sources( zyppng_async_HEADERS
  ng/async/AsyncOp
  ng/async/asyncop.h
)

zypp_add_sources( zyppng_base_SRCS
  ng/base/abstracteventsource.cc
  ng/base/base.cc
  ng/base/eventdispatcher_glib.cc
  ng/base/eventloop_glib.cc
  ng/base/linuxhelpers.cc
  ng/base/timer.cc
  ng/base/threaddata.cc
  ng/base/socketnotifier.cc
  ng/base/unixsignalsource.cpp
)

zypp_add_sources( zyppng_base_HEADERS
  ng/base/AbstractEventSource
  ng/base/abstracteventsource.h
  ng/base/AutoDisconnect
  ng/base/autodisconnect.h
  ng/base/Base
  ng/base/base.h
  ng/base/EventDispatcher
  ng/base/eventdispatcher.h
  ng/base/EventLoop
  ng/base/eventloop.h
  ng/base/signals.h
  ng/base/SocketNotifier
  ng/base/socketnotifier.h
  ng/base/statemachine.h
  ng/base/Timer
  ng/base/timer.h
  ng/base/unixsignalsource.h
  ng/base/zyppglobal.h
)

zypp_add_sources( zyppng_base_private_HEADERS
  ng/base/private/abstracteventsource_p.h
  ng/base/private/base_p.h
  ng/base/private/eventdispatcher_glib_p.h
  ng/base/private/eventloop_glib_p.h
  ng/base/private/linuxhelpers_p.h
  ng/base/private/threaddata_p.h
)

zypp_add_sources( zyppng_core_HEADERS
  ng/core/bytearray.h
  ng/core/ByteArray
  ng/core/url.h
  ng/core/Url
  ng/core/idstring.h
  ng/core/IdString
  ng/core/string.h
  ng/core/String
)

zypp_add_sources( zyppng_io_private_HEADERS
  ng/io/private/asyncdatasource_p.h
  ng/io/private/iobuffer_p.h
  ng/io/private/iodevice_p.h
  ng/io/private/socket_p.h
  ng/io/private/sockaddr_p.h
  ng/io/private/abstractspawnengine_p.h
  ng/io/private/forkspawnengine_p.h
)

zypp_add_sources( zyppng_io_SRCS
  ng/io/asyncdatasource.cpp
  ng/io/iobuffer.cc
  ng/io/iodevice.cc
  ng/io/process.cpp
  ng/io/socket.cc
  ng/io/sockaddr.cpp
  ng/io/abstractspawnengine.cc
  ng/io/forkspawnengine.cc
)

zypp_add_sources( zyppng_io_HEADERS
  ng/io/AsyncDataSource
  ng/io/asyncdatasource.h
  ng/io/IODevice
  ng/io/iodevice.h
  ng/io/Process
  ng/io/process.h
  ng/io/Socket
  ng/io/socket.h
  ng/io/sockaddr.h
  ng/io/SockAddr
)

zypp_add_sources( zyppng_meta_HEADERS
  ng/meta/Functional
  ng/meta/functional.h
  ng/meta/FunctionTraits
  ng/meta/function_traits.h
  ng/meta/TypeTraits
  ng/meta/type_traits.h
)

zypp_add_sources( zyppng_pipelines_HEADERS
  ng/pipelines/Algorithm
  ng/pipelines/algorithm.h
  ng/pipelines/AsyncResult
  ng/pipelines/asyncresult.h
  ng/pipelines/Await
  ng/pipelines/await.h
  ng/pipelines/Expected
  ng/pipelines/expected.h
  ng/pipelines/Lift
  ng/pipelines/lift.h
  ng/pipelines/MTry
  ng/pipelines/mtry.h
  ng/pipelines/Redo
  ng/pipelines/redo.h
  ng/pipelines/Transform
  ng/pipelines/transform.h
  ng/pipelines/Wait
  ng/pipelines/wait.h
)

zypp_add_sources( zyppng_rpc_HEADERS
  ng/rpc/stompframestream.h
)

zypp_add_sources( zyppng_rpc_SRCS
  ng/rpc/stompframestream.cc
)

zypp_add_sources( zyppng_thread_SRCS
  ng/thread/asyncqueue.cc
  ng/thread/wakeup.cpp
)

zypp_add_sources( zyppng_thread_HEADERS
  ng/thread/AsyncQueue
  ng/thread/asyncqueue.h
  ng/thread/private/asyncqueue_p.h
  ng/thread/Wakeup
  ng/thread/wakeup.h
)

zypp_add_sources( zyppng_thread_private_HEADERS
)

zypp_add_sources( zyppng_ui_SRCS
  ng/ui/progressobserver.cc
  ng/ui/userinterface.cc
  ng/ui/userrequest.cc
)

zypp_add_sources( zyppng_ui_HEADERS
  ng/ui/progressobserver.h
  ng/ui/ProgressObserver
  ng/ui/userinterface.h
  ng/ui/UserInterface
  ng/ui/userrequest.h
  ng/ui/UserRequest
)

zypp_add_sources( zyppng_ui_private_HEADERS
  ng/ui/private/userinterface_p.h
)


SET( zypp_core_SOURCES
  ${arg_SOURCES}
  ${zypp_toplevel_SRCS}
  ${zypp_base_SRCS}
  ${zypp_fs_SRCS}
  ${zypp_rpc_SRCS}
  ${zypp_ui_SRCS}
  ${zypp_url_SRCS}
  ${zypp_parser_SRCS}
  ${zypp_parser_xml_SRCS}
  ${zyppng_base_SRCS}
  ${zyppng_io_SRCS}
  ${zyppng_rpc_SRCS}
  ${zyppng_thread_SRCS}
  ${zyppng_ui_SRCS}
)

SET( zypp_core_HEADERS
  ${CMAKE_CURRENT_BINARY_DIR}/APIConfig.h
  ${arg_HEADERS}
  ${zypp_toplevel_headers}
  ${zyppng_async_HEADERS}
  ${zypp_base_HEADERS}
  ${zypp_fs_HEADERS}
  ${zypp_rpc_HEADERS}
  ${zypp_ui_HEADERS}
  ${zypp_url_HEADERS}
  ${zypp_parser_HEADERS}
  ${zypp_parser_private_HEADERS}
  ${zypp_parser_xml_HEADERS}
  ${zyppng_base_HEADERS}
  ${zyppng_base_private_HEADERS}
  ${zyppng_core_HEADERS}
  ${zyppng_io_HEADERS}
  ${zyppng_io_private_HEADERS}
  ${zyppng_meta_HEADERS}
  ${zyppng_pipelines_HEADERS}
  ${zyppng_rpc_HEADERS}
  ${zyppng_thread_HEADERS}
  ${zyppng_thread_private_HEADERS}
  ${zyppng_ui_HEADERS}
  ${zyppng_ui_private_HEADERS}
)

# Default loggroup for all files
SET_LOGGROUP( "zypp-core" ${zypp_core_SOURCES} )

# System libraries
SET(UTIL_LIBRARY util)

if ( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

  if (ZYPP_CXX_CLANG_TIDY)
    set( CMAKE_CXX_CLANG_TIDY ${ZYPP_CXX_CLANG_TIDY} )
  endif(ZYPP_CXX_CLANG_TIDY)

  if (ZYPP_CXX_CPPCHECK)
    set(CMAKE_CXX_CPPCHECK ${ZYPP_CXX_CPPCHECK})
  endif(ZYPP_CXX_CPPCHECK)

endif( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

get_property(COMP_DEFS DIRECTORY PROPERTY COMPILE_DEFINITIONS )
message("Comp defs for ${arg_TARGETNAME}: ${COMP_DEFS}")

ADD_LIBRARY( ${arg_TARGETNAME} STATIC ${zypp_core_SOURCES} ${zypp_core_HEADERS} )

target_link_libraries( ${arg_TARGETNAME} PRIVATE ${arg_FLAGS} )
target_link_libraries( ${arg_TARGETNAME} INTERFACE ${LIBGLIB_LIBRARIES} )
target_link_libraries( ${arg_TARGETNAME} INTERFACE ${OPENSSL_LIBRARIES} )
target_link_libraries( ${arg_TARGETNAME} INTERFACE ${CRYPTO_LIBRARIES} )
target_link_libraries( ${arg_TARGETNAME} INTERFACE pthread )
target_link_libraries( ${arg_TARGETNAME} INTERFACE ${SIGCPP_LIBRARIES} )
TARGET_LINK_LIBRARIES( ${arg_TARGETNAME} INTERFACE ${ZLIB_LIBRARY} )
TARGET_LINK_LIBRARIES( ${arg_TARGETNAME} INTERFACE ${UTIL_LIBRARY} )

IF (ENABLE_ZSTD_COMPRESSION)
  TARGET_LINK_LIBRARIES( ${arg_TARGETNAME} INTERFACE ${ZSTD_LIBRARY})
ENDIF (ENABLE_ZSTD_COMPRESSION)

IF (ENABLE_ZCHUNK_COMPRESSION)
  TARGET_LINK_LIBRARIES( ${arg_TARGETNAME} INTERFACE ${ZCHUNK_LDFLAGS})
ENDIF(ENABLE_ZCHUNK_COMPRESSION)

zypp_logic_setup_includes()

endfunction()
