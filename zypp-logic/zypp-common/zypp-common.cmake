include(${CMAKE_CURRENT_LIST_DIR}/../logic-helper.cmake)

function(zypp_add_common_target)

  set(options INSTALL_HEADERS )
  set(oneValueArgs TARGETNAME)
  set(multiValueArgs HEADERS SOURCES )

  cmake_parse_arguments(PARSE_ARGV 0 arg "${options}" "${oneValueArgs}" "${multiValueArgs}")

  ADD_DEFINITIONS( -DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale" -DTEXTDOMAIN="zypp" -DZYPP_DLL )

  zypp_add_sources( zypp_common_base_HEADERS
    base/DrunkenBishop.h
  )

  zypp_add_sources( zypp_common_base_SRCS
    base/DrunkenBishop.cc
  )

if( ${arg_INSTALL_HEADERS} )
  INSTALL( FILES ${zypp_common_base_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-common/base" )
endif()


  zypp_add_sources ( zypp_common_HEADERS
    KeyManager.h
    KeyRingException.h
    PublicKey.h
  )

  zypp_add_sources ( zypp_common_SRCS
    KeyManager.cc
    PublicKey.cc
  )

  zypp_add_sources ( zypp_common_private_HEADERS
    private/keyring_p.h
  )

  zypp_add_sources ( zypp_common_private_SRCS
    private/keyring_p.cc
  )

if( ${arg_INSTALL_HEADERS} )
  INSTALL(  FILES ${zypp_common_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-common" )
endif()


  SET( zypp_common_lib_SRCS
    ${arg_SOURCES}
    ${zypp_common_base_SRCS}
    ${zypp_common_SRCS}
    ${zypp_common_private_SRCS}
  )

  SET( zypp_common_lib_HEADERS
    ${arg_HEADERS}
    ${zypp_common_base_HEADERS}
    ${zypp_common_HEADERS}
    ${zypp_common_private_HEADERS}
  )

  # Default loggroup for all files
  SET_LOGGROUP( "zypp" ${zypp_common_lib_SRCS} )

  if ( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )
    if (ZYPP_CXX_CLANG_TIDY)
      set( CMAKE_CXX_CLANG_TIDY ${ZYPP_CXX_CLANG_TIDY} )
    endif(ZYPP_CXX_CLANG_TIDY)

    if (ZYPP_CXX_CPPCHECK)
      set(CMAKE_CXX_CPPCHECK ${ZYPP_CXX_CPPCHECK})
    endif(ZYPP_CXX_CPPCHECK)
  endif( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

  add_library( ${arg_TARGETNAME} STATIC ${zypp_common_lib_SRCS} ${zypp_common_lib_HEADERS} )
  target_link_libraries( ${arg_TARGETNAME} PRIVATE zypp_lib_compiler_flags )
  target_link_libraries( ${arg_TARGETNAME} INTERFACE ${GPGME_LIBRARIES} )

  zypp_logic_setup_includes()

endfunction()
