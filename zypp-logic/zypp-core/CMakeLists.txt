INCLUDE_DIRECTORIES ( ${LIBZYPP_SOURCE_DIR} )

ADD_DEFINITIONS( -DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale" -DTEXTDOMAIN="zypp" -DZYPP_DLL )

CONFIGURE_FILE ( APIConfig.h.in APIConfig.h @ONLY )

SET( zypp_toplevel_headers
  ${CMAKE_CURRENT_BINARY_DIR}/APIConfig.h
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

SET( zypp_toplevel_SRCS
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

if( ZYPP_LOGIC_INSTALL_CXX )
  INSTALL(  FILES ${zypp_toplevel_headers} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core" )
endif()

SET( zypp_base_HEADERS
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

SET( zypp_base_SRCS
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

  list( APPEND zypp_base_SRCS
    base/zckstream.cc
  )

  list( APPEND zypp_base_HEADERS
    base/ZckStream
    base/zckstream.h
  )

ENDIF(ENABLE_ZCHUNK_COMPRESSION)

if( ZYPP_LOGIC_INSTALL_CXX )
  INSTALL(  FILES ${zypp_base_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/base" )
endif()


SET( zypp_fs_SRCS
  fs/PathInfo.cc
  fs/TmpPath.cc
)

SET( zypp_fs_HEADERS
  fs/PathInfo.h
  fs/TmpPath.h
  fs/WatchFile
  fs/watchfile.h
)

if( ZYPP_LOGIC_INSTALL_CXX )
  INSTALL(  FILES ${zypp_fs_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/fs" )
endif()


SET( zypp_rpc_SRCS
  rpc/PluginFrame.cc
  rpc/PluginFrameException.cc
)

SET( zypp_rpc_HEADERS
  rpc/PluginFrame.h
  rpc/PluginFrameException.h
)

if( ZYPP_LOGIC_INSTALL_CXX )
  INSTALL(  FILES ${zypp_rpc_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/rpc" )
endif()


SET( zypp_ui_SRCS
  ui/progressdata.cc
)

SET( zypp_ui_HEADERS
  ui/ProgressData
  ui/progressdata.h
)

if( ZYPP_LOGIC_INSTALL_CXX )
  INSTALL(  FILES ${zypp_ui_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/ui" )
endif()


SET( zypp_url_SRCS
  url/UrlUtils.cc
  url/UrlBase.cc
)

SET( zypp_url_HEADERS
  url/UrlBase.h
  url/UrlException.h
  url/UrlUtils.h
)

if( ZYPP_LOGIC_INSTALL_CXX )
  INSTALL(  FILES ${zypp_url_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/url" )
endif()

SET( zypp_parser_SRCS
  parser/iniparser.cc
  parser/inidict.cc
  parser/json.cc
  parser/json/JsonValue.cc
  parser/parseexception.cc
  parser/sysconfig.cc
)

SET ( zypp_parser_private_HEADERS
  parser/json/JsonBool.h
  parser/json/JsonNull.h
  parser/json/JsonNumber.h
  parser/json/JsonString.h
  parser/json/JsonValue.h
)

SET( zypp_parser_HEADERS
  parser/IniParser
  parser/iniparser.h
  parser/IniDict
  parser/inidict.h
  parser/json.h
  parser/ParseException
  parser/parseexception.h
  parser/Sysconfig
  parser/sysconfig.h
)

if( ZYPP_LOGIC_INSTALL_CXX )
  INSTALL(  FILES ${zypp_parser_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/parser" )
endif()

SET( zypp_parser_xml_SRCS
  parser/xml/XmlEscape.cc
)

SET( zypp_parser_xml_HEADERS
  parser/xml/XmlEscape.h
)

if( ZYPP_LOGIC_INSTALL_CXX )
  INSTALL(  FILES ${zypp_parser_xml_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-core/parser/xml" )
endif()

SET( zyppng_async_HEADERS
  ng/async/AsyncOp
  ng/async/asyncop.h
)

SET( zyppng_base_SRCS
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

SET( zyppng_base_HEADERS
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

SET( zyppng_base_private_HEADERS
  ng/base/private/abstracteventsource_p.h
  ng/base/private/base_p.h
  ng/base/private/eventdispatcher_glib_p.h
  ng/base/private/eventloop_glib_p.h
  ng/base/private/linuxhelpers_p.h
  ng/base/private/threaddata_p.h
)

SET( zyppng_core_HEADERS
  ng/core/bytearray.h
  ng/core/ByteArray
  ng/core/url.h
  ng/core/Url
  ng/core/idstring.h
  ng/core/IdString
  ng/core/string.h
  ng/core/String
)

SET( zyppng_io_private_HEADERS
  ng/io/private/asyncdatasource_p.h
  ng/io/private/iobuffer_p.h
  ng/io/private/iodevice_p.h
  ng/io/private/socket_p.h
  ng/io/private/sockaddr_p.h
  ng/io/private/abstractspawnengine_p.h
  ng/io/private/forkspawnengine_p.h
)

SET( zyppng_io_SRCS
  ng/io/asyncdatasource.cpp
  ng/io/iobuffer.cc
  ng/io/iodevice.cc
  ng/io/process.cpp
  ng/io/socket.cc
  ng/io/sockaddr.cpp
  ng/io/abstractspawnengine.cc
  ng/io/forkspawnengine.cc
)

SET( zyppng_io_HEADERS
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

SET( zyppng_meta_HEADERS
  ng/meta/Functional
  ng/meta/functional.h
  ng/meta/FunctionTraits
  ng/meta/function_traits.h
  ng/meta/TypeTraits
  ng/meta/type_traits.h
)

SET( zyppng_pipelines_HEADERS
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

SET( zyppng_rpc_HEADERS
  ng/rpc/stompframestream.h
)

SET( zyppng_rpc_SRCS
  ng/rpc/stompframestream.cc
)

SET( zyppng_thread_SRCS
  ng/thread/asyncqueue.cc
  ng/thread/wakeup.cpp
)

SET( zyppng_thread_HEADERS
  ng/thread/AsyncQueue
  ng/thread/asyncqueue.h
  ng/thread/private/asyncqueue_p.h
  ng/thread/Wakeup
  ng/thread/wakeup.h
)

SET( zyppng_thread_private_HEADERS
)

SET( zyppng_ui_SRCS
  ng/ui/progressobserver.cc
  ng/ui/userinterface.cc
  ng/ui/userrequest.cc
)

SET( zyppng_ui_HEADERS
  ng/ui/progressobserver.h
  ng/ui/ProgressObserver
  ng/ui/userinterface.h
  ng/ui/UserInterface
  ng/ui/userrequest.h
  ng/ui/UserRequest
)

SET( zyppng_ui_private_HEADERS
  ng/ui/private/userinterface_p.h
)

SET ( zypp_HEADERS
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

SET ( zypp_SRCS
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

# Default loggroup for all files
SET_LOGGROUP( "zypp-core" ${zypp_SRCS} )

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

ADD_LIBRARY( ${ZYPP_LOGIC_TARGET_PREFIX}-core STATIC ${zypp_SRCS} ${zypp_HEADERS} )

target_link_libraries( ${ZYPP_LOGIC_TARGET_PREFIX}-core PRIVATE zypp_lib_compiler_flags )
target_link_libraries( ${ZYPP_LOGIC_TARGET_PREFIX}-core INTERFACE ${LIBGLIB_LIBRARIES} )
target_link_libraries( ${ZYPP_LOGIC_TARGET_PREFIX}-core INTERFACE ${OPENSSL_LIBRARIES} )
target_link_libraries( ${ZYPP_LOGIC_TARGET_PREFIX}-core INTERFACE ${CRYPTO_LIBRARIES} )
target_link_libraries( ${ZYPP_LOGIC_TARGET_PREFIX}-core INTERFACE pthread )
target_link_libraries( ${ZYPP_LOGIC_TARGET_PREFIX}-core INTERFACE ${SIGCPP_LIBRARIES} )
TARGET_LINK_LIBRARIES( ${ZYPP_LOGIC_TARGET_PREFIX}-core INTERFACE ${ZLIB_LIBRARY} )
TARGET_LINK_LIBRARIES( ${ZYPP_LOGIC_TARGET_PREFIX}-core INTERFACE ${UTIL_LIBRARY} )

IF (ENABLE_ZSTD_COMPRESSION)
  TARGET_LINK_LIBRARIES( ${ZYPP_LOGIC_TARGET_PREFIX}-core INTERFACE ${ZSTD_LIBRARY})
ENDIF (ENABLE_ZSTD_COMPRESSION)

IF (ENABLE_ZCHUNK_COMPRESSION)
  TARGET_LINK_LIBRARIES( ${ZYPP_LOGIC_TARGET_PREFIX}-core INTERFACE ${ZCHUNK_LDFLAGS})
ENDIF(ENABLE_ZCHUNK_COMPRESSION)

get_filename_component(PARENT_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
get_filename_component(PARENT_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR} DIRECTORY)
target_include_directories( ${ZYPP_LOGIC_TARGET_PREFIX}-core  INTERFACE
  $<BUILD_INTERFACE:${PARENT_SRC_DIR}>
  $<BUILD_INTERFACE:${PARENT_BIN_DIR}>
)
