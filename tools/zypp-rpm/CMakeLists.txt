PROJECT( zypp-rpm C CXX )

set (CMAKE_CXX_STANDARD 17)
SET (CMAKE_CXX_EXTENSIONS OFF)

FIND_PACKAGE(Rpm REQUIRED)
IF ( NOT RPM_FOUND)
  MESSAGE( FATAL_ERROR " rpm-devel not found" )
ELSE ( NOT RPM_FOUND)

  INCLUDE_DIRECTORIES(${RPM_INCLUDE_DIR})
  # fix includes not relative to rpm
  INCLUDE_DIRECTORIES(${RPM_INCLUDE_DIR}/rpm)

  # rpm verify function and callback states were introduced in rpm-4.15
  if( RPM_LIB_VER VERSION_GREATER_EQUAL "4.15.0"  AND  RPM_LIB_VER VERSION_LESS "5.0.0")
    ADD_DEFINITIONS( -DHAVE_RPM_VERIFY_TRANSACTION_STEP )
  endif()

  if( RPM_LIB_VER VERSION_GREATER_EQUAL "5.0.0" )
        MESSAGE( STATUS "rpm found: enable rpm-4 compat interface." )
        ADD_DEFINITIONS(-D_RPM_5)
  endif ()
ENDIF( NOT RPM_FOUND)

SET( SOURCES
  errorcodes.h
  main.cc
  BinHeader.h
  BinHeader.cc
)

if ( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )
  if (ZYPP_CXX_CLANG_TIDY)
    set( CMAKE_CXX_CLANG_TIDY ${ZYPP_CXX_CLANG_TIDY} )
  endif(ZYPP_CXX_CLANG_TIDY)

  if (ZYPP_CXX_CPPCHECK)
    set(CMAKE_CXX_CPPCHECK ${ZYPP_CXX_CPPCHECK})
  endif(ZYPP_CXX_CPPCHECK)
endif( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

add_executable( ${PROJECT_NAME} ${SOURCES} )
target_link_libraries( ${PROJECT_NAME} zypp_exe_compiler_flags )
target_link_libraries( ${PROJECT_NAME} commit-proto-obj )
target_link_libraries( ${PROJECT_NAME} zypp-core )
target_link_libraries( ${PROJECT_NAME} util )
target_link_libraries( ${PROJECT_NAME} ${RPM_LIBRARY} ${RPMIO_LIBRARY} )

INSTALL( TARGETS ${PROJECT_NAME}	DESTINATION "${ZYPP_LIBEXEC_INSTALL_DIR}" )
