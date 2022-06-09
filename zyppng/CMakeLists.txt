PROJECT( zyppng C CXX )

set (CMAKE_CXX_STANDARD 17)
SET (CMAKE_CXX_EXTENSIONS OFF)

# Collect all files that may contain translatable texts
FILE( GLOB_RECURSE POT_FILE_DEPENDS RELATIVE ${LIBZYPP_SOURCE_DIR} "*.h" "*.cc" )
SET( POT_FILE_DEPENDS_ZYPP_MEDIA ${POT_FILE_DEPENDS} PARENT_SCOPE )

INCLUDE_DIRECTORIES ( ${LIBZYPP_SOURCE_DIR} )

ADD_DEFINITIONS( -DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale" -DTEXTDOMAIN="zypp" -DZYPP_DLL )

SET( zyppng_HEADERS
  context.h
)

SET( zyppng_private_HEADERS
)

SET( zyppng_SRCS
  context.cc
)

INSTALL(  FILES ${zypp_media_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zyppng" )

SET( zyppng_lib_SRCS
    ${zyppng_SRCS}
)
SET( zyppng_lib_HEADERS
    ${zyppng_private_HEADERS} ${zyppng_HEADERS}
)

# Default loggroup for all files
SET_LOGGROUP( "zyppng" ${zyppng_lib_SRCS} )

ADD_LIBRARY( zyppng SHARED  ${zyppng_lib_SRCS} ${zyppng_lib_HEADERS} )
TARGET_LINK_LIBRARIES( zyppng zypp )
