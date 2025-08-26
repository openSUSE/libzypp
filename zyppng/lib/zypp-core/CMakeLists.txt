include(${zypp-libs_SOURCE_DIR}/zypp-logic/zypp-core/zypp-core.cmake)

SET( VERSION "${VERSION} (zyppng)" )

zypp_add_core_target( TARGETNAME "zyppng-core" FLAGS zyppng_lib_compiler_flags )
