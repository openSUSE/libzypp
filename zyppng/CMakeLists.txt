project(zyppng)

SET ( CMAKE_CXX_STANDARD 20 )
SET ( CMAKE_CXX_EXTENSIONS OFF )


SET ( zyppng_lib_SRCS )

# Default loggroup for all files
#SET_LOGGROUP( "zypp" ${zyppng_lib_SRCS} )

if ( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )
  message(NOTICE "Enabling static analysis: ${ZYPP_CXX_CLANG_TIDY} ${ZYPP_CXX_CPPCHECK}")

  if (ZYPP_CXX_CLANG_TIDY)
    set( CMAKE_CXX_CLANG_TIDY ${ZYPP_CXX_CLANG_TIDY} )
  endif(ZYPP_CXX_CLANG_TIDY)

  if (ZYPP_CXX_CPPCHECK)
    set(CMAKE_CXX_CPPCHECK ${ZYPP_CXX_CPPCHECK})
  endif(ZYPP_CXX_CPPCHECK)
endif( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )


SET( ZYPP_LOGIC_TARGET_PREFIX "zyppng" )
SET( ZYPP_LOGIC_INSTALL_CXX   false    )
include_directories(${zypp-libs_SOURCE_DIR}/zypp-logic)
ADD_SUBDIRECTORY( ${zypp-libs_SOURCE_DIR}/zypp-logic ${CMAKE_CURRENT_BINARY_DIR}/zypp-logic )
