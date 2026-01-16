include(${zypp-libs_SOURCE_DIR}/zypp-logic/zypp-core/zypp-core.cmake)

SET( zyppng_ng_async_HEADERS
  ng/async/awaitable.h
  ng/async/iotask.h
  ng/async/task.h
  ng/pipelines/operators.h
)

SET( zyppng_ng_async_SOURCES

)



zypp_add_core_target( TARGETNAME "zypp-core" FLAGS zypp_lib_compiler_flags HEADERS ${zyppng_ng_async_HEADERS} SOURCES ${zyppng_ng_async_SOURCES} INSTALL_HEADERS )


