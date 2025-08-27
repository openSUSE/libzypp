include(${zypp-libs_SOURCE_DIR}/zypp-logic/shared/shared.cmake)
zypp_add_shared_targets( TARGETPREFIX "zyppng" FLAGS zyppng_lib_compiler_flags )

if(TRUE)
  set( arg_TARGETNAME "tvm-protocol-obj" )

  ADD_LIBRARY( ${arg_TARGETNAME} OBJECT tvm/tvmsettings.h tvm/tvmsettings.cc )
  target_link_libraries( ${arg_TARGETNAME} PRIVATE zyppng_lib_compiler_flags )

  get_filename_component(PARENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}   DIRECTORY)
  get_filename_component(PARENT_BIN_DIR  ${CMAKE_CURRENT_BINARY_DIR} DIRECTORY)
  target_include_directories( ${arg_TARGETNAME} INTERFACE
    $<BUILD_INTERFACE:${PARENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PARENT_BIN_DIR}>
    ${zypp-libs_SOURCE_DIR}/zypp-logic
  )

  target_include_directories( ${arg_TARGETNAME} PRIVATE
    ${PARENT_SOURCE_DIR}
    ${PARENT_BIN_DIR}
    ${zypp-libs_SOURCE_DIR}/zypp-logic
  )
endif()
