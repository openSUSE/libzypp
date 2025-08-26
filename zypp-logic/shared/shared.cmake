# This subproject contains small shared object libraries
# usually used in libzypp and workers, or for tests.

include(${CMAKE_CURRENT_LIST_DIR}/../logic-helper.cmake)


function( zypp_add_shared_targets )

  set(oneValueArgs TARGETPREFIX FLAGS )
  set(multiValueArgs HEADERS SOURCES )

  cmake_parse_arguments(PARSE_ARGV 0 arg "" "${oneValueArgs}" "${multiValueArgs}")

  ADD_DEFINITIONS( -DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale" -DTEXTDOMAIN="zypp" -DZYPP_DLL )

  # ------------------------rpm protocol lib ----------------------------
  # rpm protocol definition, could be moved to a zypp-rpm lib some day
  # if more than one class is required to be shared between libzypp and zypp-rpm

  block()
    #initialize arg_TARGETNAME because the zypp_logic_setup_includes macro needs it
    set( arg_TARGETNAME "commit-proto-obj" )
    if ( arg_TARGETPREFIX )
      #use arg_TARGETNAME because the zypp_logic_setup_includes macro needs it
      set( arg_TARGETNAME "${arg_TARGETPREFIX}-${arg_TARGETNAME}" )
    endif()

    zypp_add_sources( COMMIT_SRCS commit/CommitMessages.h commit/CommitMessages.cc )
    ADD_LIBRARY( ${arg_TARGETNAME} OBJECT ${COMMIT_SRCS} )
    target_link_libraries( ${arg_TARGETNAME} PRIVATE ${arg_FLAGS} )
    zypp_logic_setup_includes()
  endblock()


endfunction()

