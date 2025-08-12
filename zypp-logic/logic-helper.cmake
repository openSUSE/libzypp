# - Shared logic code helper
# Providers helper functions and macros for zypp-logic projects
# that can be included as part of a zypp or zyppng library
#
#  zypp_logic_prefix_sources - Prefixes a list of source FILES with the given PREFIXDIR
#  zypp_add_sources          - Convenience macro that automtically calls zypp_logic_prefix_sources
#  zypp_logic_setup_includes - Set's the required include paths' for a zypp_logic target,
#                              requires arg_TARGETNAME to be already set to the targetname before calling


function(zypp_logic_prefix_sources)
  set(oneValueArgs PREFIXDIR OUT )
  set(multiValueArgs FILES )

  cmake_parse_arguments(PARSE_ARGV 0 arg "" "${oneValueArgs}" "${multiValueArgs}")

  foreach( curr ${arg_FILES} )
    list( APPEND ${arg_OUT} "${arg_PREFIXDIR}/${curr}" )
  endforeach()

  set( ${arg_OUT} ${${arg_OUT}} PARENT_SCOPE )
endfunction()


macro(zypp_add_sources var)

  zypp_logic_prefix_sources(
    PREFIXDIR "${CMAKE_CURRENT_FUNCTION_LIST_DIR}"
    OUT ${var}
    FILES ${ARGN}
  )

endmacro()


macro( zypp_logic_setup_includes )
  get_filename_component(PARENT_LIST_DIR ${CMAKE_CURRENT_FUNCTION_LIST_DIR}   DIRECTORY)
  get_filename_component(PARENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}   DIRECTORY)
  get_filename_component(PARENT_BIN_DIR  ${CMAKE_CURRENT_BINARY_DIR} DIRECTORY)

  target_include_directories( ${arg_TARGETNAME} INTERFACE
    $<BUILD_INTERFACE:${PARENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${PARENT_LIST_DIR}>
    $<BUILD_INTERFACE:${PARENT_BIN_DIR}>
  )

  target_include_directories( ${arg_TARGETNAME} PRIVATE
    ${PARENT_SOURCE_DIR}
    ${PARENT_LIST_DIR}
  )
endmacro()
