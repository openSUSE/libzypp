include(${CMAKE_CURRENT_LIST_DIR}/../logic-helper.cmake)

function(zypp_add_zypp_target)

  FIND_PACKAGE(Rpm REQUIRED)
  IF ( NOT RPM_FOUND)
    MESSAGE( FATAL_ERROR " rpm-devel not found" )
  ENDIF ( NOT RPM_FOUND)

  set(options INSTALL_HEADERS )
  set(oneValueArgs TARGETNAME FLAGS )
  set(multiValueArgs HEADERS SOURCES )

  cmake_parse_arguments(PARSE_ARGV 0 arg "${options}" "${oneValueArgs}" "${multiValueArgs}")

  zypp_add_sources( zypp_SRCS
    CapMatch.cc
    CpeId.cc
    Dep.cc
    Edition.cc
    IdString.cc
    Range.cc
    Rel.cc
    ResKind.cc
  )

  zypp_add_sources( zypp_EARLY_SRCS
    Arch.cc
    CountryCode.cc
    LanguageCode.cc
    Locale.cc
  )

  zypp_add_sources( zypp_HEADERS
    Arch.h
    Bit.h
    CapMatch.h
    CountryCode.h
    CpeId.h
    Dep.h
    Edition.h
    IdString.h
    IdStringType.h
    LanguageCode.h
    Locale.h
    Range.h
    RelCompare.h
    Rel.h
    ResKind.h
    ResTraits.h
    ResolverNamespace.h
  )

  if( ${arg_INSTALL_HEADERS} )
    INSTALL(  FILES
      ${zypp_HEADERS}
      DESTINATION ${INCLUDE_INSTALL_DIR}/zypp
    )
  endif()

  zypp_add_sources( zypp_base_SRCS
    base/SerialNumber.cc
    base/SetRelationMixin.cc
    base/StrMatcher.cc
  )

  zypp_add_sources( zypp_base_HEADERS
    base/SerialNumber.h
    base/SetRelationMixin.h
    base/SetTracker.h
    base/StrMatcher.h
  )

  if( ${arg_INSTALL_HEADERS} )
    INSTALL(  FILES
      ${zypp_base_HEADERS}
      DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/base
    )
  endif()

  zypp_add_sources( zypp_sat_SRCS
    sat/SolvAttr.cc
  )

  zypp_add_sources( zypp_sat_HEADERS
    sat/SolvAttr.h
  )

  if( ${arg_INSTALL_HEADERS} )
    INSTALL(  FILES
      ${zypp_sat_HEADERS}
      DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/sat
    )
  endif()


  zypp_add_sources( zypp_sat_detail_SRCS
    #sat/detail/PoolImpl.cc
  )

  zypp_add_sources( zypp_sat_detail_HEADERS
    #sat/detail/PoolImpl.h
    sat/detail/PoolDefines.h
    sat/detail/PoolMember.h
  )

  if( ${arg_INSTALL_HEADERS} )
    INSTALL(  FILES
      ${zypp_sat_detail_HEADERS}
      DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/sat/detail
    )
  endif()

  zypp_add_sources( zyppng_sat_SRCS
    ng/sat/stringpool.cc
  )

  zypp_add_sources( zyppng_sat_HEADERS
    ng/sat/poolconstants.h
    ng/sat/stringpool.h
  )

  SET( zypp_lib_SRCS
    ${arg_SOURCES}
    ${zypp_SRCS}
    ${zypp_sat_SRCS}
    ${zypp_sat_detail_SRCS}
    ${zyppng_sat_SRCS}

    ${zypp_EARLY_SRCS}
    ${zypp_base_SRCS}
  )

  SET( zypp_lib_HEADERS
    ${arg_HEADERS}
    ${zypp_HEADERS}
    ${zypp_base_HEADERS}
    ${zypp_sat_HEADERS}
    ${zypp_sat_detail_HEADERS}
    ${zyppng_sat_HEADERS}
  )

  # Default loggroup for all files
  SET_LOGGROUP( "zypp" ${zypp_lib_SRCS} )

  # override some defaults
  # SET_LOGGROUP( "libsolv" ${zypp_sat_SRCS} )

  if ( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

    message(NOTICE "Enabling static analysis: ${ZYPP_CXX_CLANG_TIDY} ${ZYPP_CXX_CPPCHECK}")

    set_source_files_properties(
        target/TargetImpl.cc
        PROPERTIES
        SKIP_LINTING ON
    )

    if (ZYPP_CXX_CLANG_TIDY)
      set( CMAKE_CXX_CLANG_TIDY ${ZYPP_CXX_CLANG_TIDY} )
    endif(ZYPP_CXX_CLANG_TIDY)

    if (ZYPP_CXX_CPPCHECK)
      set(CMAKE_CXX_CPPCHECK ${ZYPP_CXX_CPPCHECK})
    endif(ZYPP_CXX_CPPCHECK)

  endif( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

  ADD_LIBRARY( ${arg_TARGETNAME} OBJECT ${zypp_lib_SRCS} ${zypp_lib_HEADERS}  )

  zypp_logic_setup_includes()

  target_link_libraries( ${arg_TARGETNAME} ${arg_FLAGS} )

  target_include_directories( ${arg_TARGETNAME} PUBLIC ${RPM_INCLUDE_DIR})

  IF (ENABLE_ZCHUNK_COMPRESSION)
    target_include_directories( ${arg_TARGETNAME} PRIVATE ${ZCHUNK_INCLUDEDIR} )
  ENDIF(ENABLE_ZCHUNK_COMPRESSION)

  # fix includes not relative to rpm
  target_include_directories( ${arg_TARGETNAME} PUBLIC ${RPM_INCLUDE_DIR}/rpm)

  # rpm verify function and callback states were introduced in rpm-4.15
  if( ( RPM_LIB_VER VERSION_GREATER_EQUAL "4.15.0"  AND  RPM_LIB_VER VERSION_LESS "5.0.0" ) OR RPM_LIB_VER VERSION_GREATER_EQUAL "6.0.0" )
    target_compile_definitions( ${arg_TARGETNAME} PUBLIC HAVE_RPM_VERIFY_TRANSACTION_STEP )
  endif()

  if( RPM_LIB_VER VERSION_GREATER_EQUAL "5.0.0" AND RPM_LIB_VER VERSION_LESS "6.0.0" )
        MESSAGE( STATUS "rpm5 found: enable rpm-4 compat interface." )
        target_compile_definitions( ${arg_TARGETNAME} PUBLIC _RPM_5)
  endif ()


endfunction()

