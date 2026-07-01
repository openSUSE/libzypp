include(${zypp-libs_SOURCE_DIR}/zypp-logic/zypp-core/zypp-core.cmake)

zypp_configure_apiconfig(LIBZYPPGLIB "ng" "${zypp-libs_SOURCE_DIR}/zypp-logic/zypp-core/APIConfig.h.in" "${CMAKE_CURRENT_BINARY_DIR}/APIConfig.h")

SET( VERSION "${VERSION} (zyppng)" )

SET( zyppng_ng_async_HEADERS
  ng/async/awaitable.h
  ng/async/iotask.h
  ng/async/task.h
  ng/async/pipelines/await.h
  ng/async/pipelines/wait.hpp
  ng/pipelines/operators.h
)

SET( zyppng_ng_async_SOURCES


)

zypp_add_core_target( TARGETNAME "zyppng-core" FLAGS zyppng_lib_compiler_flags HEADERS ${zyppng_ng_async_HEADERS} SOURCES ${zyppng_ng_async_SOURCES} )
