include(${CMAKE_CURRENT_LIST_DIR}/../logic-helper.cmake)

function(zypp_add_media_target)

  set(options INSTALL_HEADERS )
  set(oneValueArgs TARGETNAME FLAGS )
  set(multiValueArgs HEADERS SOURCES )

  cmake_parse_arguments(PARSE_ARGV 0 arg "${options}" "${oneValueArgs}" "${multiValueArgs}")

  ADD_DEFINITIONS( -DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale" -DTEXTDOMAIN="zypp" -DZYPP_DLL )

  zypp_add_sources( zypp_media_HEADERS
    CDTools
    cdtools.h
    FileCheckException
    filecheckexception.h
    MediaConfig
    mediaconfig.h
    mediaexception.h
    MediaException
    mount.h
    Mount
  )

  zypp_add_sources( zypp_media_private_HEADERS
    ng/headervaluemap.h
    ng/HeaderValueMap
    ng/lazymediahandle.h
    ng/LazyMediaHandle
    ng/providespec.h
    ng/ProvideSpec
    ng/providefwd.h
    ng/ProvideFwd
  )

  zypp_add_sources( zypp_media_SRCS
    cdtools.cc
    filecheckexception.cc
    ng/headervaluemap.cc
    mediaconfig.cc
    mediaexception.cc
    mount.cc
    ng/providespec.cc
  )

  if( arg_INSTALL_HEADERS )
    INSTALL(  FILES ${zypp_media_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-media" )
  endif()

  zypp_add_sources( zypp_media_auth_HEADERS
    auth/AuthData
    auth/authdata.h
    auth/CredentialFileReader
    auth/credentialfilereader.h
    auth/CredentialManager
    auth/credentialmanager.h
  )

  zypp_add_sources( zypp_media_auth_private_HEADERS
  )

  zypp_add_sources( zypp_media_auth_SRCS
    auth/authdata.cc
    auth/credentialfilereader.cc
    auth/credentialmanager.cc
  )

  if( arg_INSTALL_HEADERS )
    INSTALL(  FILES ${zypp_media_auth_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-media/auth" )
  endif()

  SET( zypp_media_lib_SRCS
      ${arg_SOURCES}
      ${zypp_media_SRCS}
      ${zypp_media_auth_SRCS}
  )
  SET( zypp_media_lib_HEADERS
      ${arg_HEADERS}
      ${zypp_media_private_HEADERS} ${zypp_media_HEADERS}
      ${zypp_media_auth_private_HEADERS} ${zypp_media_auth_HEADERS}
  )

  # Default loggroup for all files
  SET_LOGGROUP( "zypp-media" ${zypp_media_lib_SRCS} )

  if ( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

    set_source_files_properties(
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ng/providemessage.cc
        PROPERTIES
        SKIP_LINTING ON
    )

    if (ZYPP_CXX_CLANG_TIDY)
      set( CMAKE_CXX_CLANG_TIDY ${ZYPP_CXX_CLANG_TIDY} )
    endif(ZYPP_CXX_CLANG_TIDY)

    if (ZYPP_CXX_CPPCHECK)
      set( CMAKE_CXX_CPPCHECK ${ZYPP_CXX_CPPCHECK} )
    endif(ZYPP_CXX_CPPCHECK)

  endif( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

  ADD_LIBRARY( ${arg_TARGETNAME} STATIC ${zypp_media_lib_SRCS} ${zypp_media_lib_HEADERS} )
  target_link_libraries( ${arg_TARGETNAME} PRIVATE  ${arg_FLAGS} )

  zypp_logic_setup_includes()

endfunction()



