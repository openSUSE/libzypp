# Collect all files that may contain translatable texts
FILE( GLOB_RECURSE POT_FILE_DEPENDS RELATIVE ${LIBZYPP_SOURCE_DIR} "*.h" "*.cc" )
SET( POT_FILE_DEPENDS_ZYPP_COMMON ${POT_FILE_DEPENDS} PARENT_SCOPE )

INCLUDE_DIRECTORIES ( ${LIBZYPP_SOURCE_DIR} )

ADD_DEFINITIONS( -DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale" -DTEXTDOMAIN="zypp" -DZYPP_DLL )

SET( zypp_common_base_HEADERS
  base/DrunkenBishop.h
)

SET( zypp_common_base_SRCS
  base/DrunkenBishop.cc
)

INSTALL( FILES ${zypp_common_base_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-common/base" )


SET ( zypp_common_HEADERS
  KeyManager.h
  KeyRingException.h
  PublicKey.h
)

SET ( zypp_common_SRCS
  KeyManager.cc
  PublicKey.cc
)

INSTALL(  FILES ${zypp_common_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp-common" )


SET( zypp_common_lib_SRCS
${zypp_common_base_SRCS}
${zypp_common_SRCS}
)

SET( zypp_common_lib_HEADERS
${zypp_common_base_HEADERS}
${zypp_common_HEADERS}
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

add_library( zypp-common STATIC ${zypp_common_lib_SRCS} ${zypp_common_lib_HEADERS} )
target_link_libraries( zypp-common PRIVATE zypp_lib_compiler_flags )
target_link_libraries( zypp-common INTERFACE ${GPGME_LIBRARIES} )
