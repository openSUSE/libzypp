
ADD_DEFINITIONS( -DTESTS_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}" -DTESTS_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}" )

ADD_SUBDIRECTORY(lib)

ENABLE_TESTING()
INCLUDE_DIRECTORIES( ${LIBZYPP_SOURCE_DIR}/tests/lib )

ADD_SUBDIRECTORY( media )
ADD_SUBDIRECTORY( zypp )
ADD_SUBDIRECTORY( parser )
ADD_SUBDIRECTORY( repo )
ADD_SUBDIRECTORY( sat )
ADD_SUBDIRECTORY( zyppng )

ADD_CUSTOM_TARGET( ctest
   COMMAND ctest -VV -a
)
