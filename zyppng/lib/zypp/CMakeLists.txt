include(${zypp-libs_SOURCE_DIR}/zypp-logic/zypp/zypp.cmake)

SET( zyppng_HEADER
  ng/Context
  ng/context.h
  ng/private/context_p.h
  ng/media/provide.h
)

SET( zyppng_SOURCES
  ng/context.cc
)

zypp_add_zypp_target( TARGETNAME zyppng-objlib HEADERS ${zyppng_HEADER} SOURCES ${zyppng_SOURCES} FLAGS zyppng_lib_compiler_flags )
