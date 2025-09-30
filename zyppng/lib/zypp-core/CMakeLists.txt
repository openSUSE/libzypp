include(${zypp-libs_SOURCE_DIR}/zypp-logic/zypp-core/zypp-core.cmake)

SET( VERSION "${VERSION} (zyppng)" )



SET( zyppng_ng_async_HEADERS
  ng/async/awaitable.h
)

SET( zyppng_ng_async_SOURCES


)


zypp_add_core_target( TARGETNAME "zyppng-core" FLAGS zyppng_lib_compiler_flags HEADERS ${zyppng_ng_async_HEADERS} SOURCES ${zyppng_ng_async_SOURCES} )
