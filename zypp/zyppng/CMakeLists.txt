SET( zyppng_base_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/base/abstracteventsource.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/base/base.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/base/eventdispatcher_glib.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/base/timer.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/base/socketnotifier.cc
)

SET( zyppng_base_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/base/AbstractEventSource
  ${CMAKE_CURRENT_SOURCE_DIR}/base/abstracteventsource.h
  ${CMAKE_CURRENT_SOURCE_DIR}/base/Base
  ${CMAKE_CURRENT_SOURCE_DIR}/base/base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/base/EventDispatcher
  ${CMAKE_CURRENT_SOURCE_DIR}/base/eventdispatcher.h
  ${CMAKE_CURRENT_SOURCE_DIR}/base/signals.h
  ${CMAKE_CURRENT_SOURCE_DIR}/base/SocketNotifier
  ${CMAKE_CURRENT_SOURCE_DIR}/base/socketnotifier.h
  ${CMAKE_CURRENT_SOURCE_DIR}/base/Timer
  ${CMAKE_CURRENT_SOURCE_DIR}/base/timer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/base/zyppglobal.h
)

SET( zyppng_base_private_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/base/private/abstracteventsource_p.h
  ${CMAKE_CURRENT_SOURCE_DIR}/base/private/base_p.h
  ${CMAKE_CURRENT_SOURCE_DIR}/base/private/eventdispatcher_glib_p.h
)


SET( zyppng_core_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/core/url.h
  ${CMAKE_CURRENT_SOURCE_DIR}/core/Url
  ${CMAKE_CURRENT_SOURCE_DIR}/core/idstring.h
  ${CMAKE_CURRENT_SOURCE_DIR}/core/IdString
)

SET( zyppng_meta_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/meta/type_traits.h
  ${CMAKE_CURRENT_SOURCE_DIR}/meta/TypeTraits
  ${CMAKE_CURRENT_SOURCE_DIR}/meta/function_traits.h
  ${CMAKE_CURRENT_SOURCE_DIR}/meta/FunctionTraits
  ${CMAKE_CURRENT_SOURCE_DIR}/meta/functional.h
  ${CMAKE_CURRENT_SOURCE_DIR}/meta/Functional
  )

SET( zyppng_monadic_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/asyncresult.h
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/AsyncResult
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/await.h
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/Await
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/expected.h
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/Expected
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/redo.h
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/Redo
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/transform.h
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/Transform
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/mtry.h
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/MTry
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/lift.h
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/Lift
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/wait.h
  ${CMAKE_CURRENT_SOURCE_DIR}/monadic/Wait
)

SET( zyppng_media_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/media/medianetwork.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/downloader.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/networkrequestdispatcher.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/networkrequesterror.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/request.cc
)

SET( zyppng_media_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/media/MediaNetwork
  ${CMAKE_CURRENT_SOURCE_DIR}/media/medianetwork.h
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/AuthData
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/authdata.h
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/downloader.h
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/networkrequestdispatcher.h
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/networkrequesterror.h
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/request.h
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/transfersettings.h
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/TransferSettings
)

SET( zyppng_media_private_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/private/downloader_p.h
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/private/networkrequestdispatcher_p.h
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/private/networkrequesterror_p.h
  ${CMAKE_CURRENT_SOURCE_DIR}/media/network/private/request_p.h
)

SET ( zyppng_HEADERS
  ${zyppng_base_HEADERS}
  ${zyppng_base_private_HEADERS}
  ${zyppng_core_HEADERS}
  ${zyppng_meta_HEADERS}
  ${zyppng_monadic_HEADERS}
  ${zyppng_media_HEADERS}
  ${zyppng_media_private_HEADERS}
  PARENT_SCOPE
)

SET ( zyppng_SRCS
  ${zyppng_base_SRCS}
  ${zyppng_media_SRCS}
  PARENT_SCOPE
)
