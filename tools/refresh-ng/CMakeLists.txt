PROJECT( refresh-ng C CXX )

set (CMAKE_CXX_STANDARD 17)
SET (CMAKE_CXX_EXTENSIONS OFF)

SET( SOURCES
  main.cc
)

ADD_DEFINITIONS( -DZYPP_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}" )

add_executable( ${PROJECT_NAME} ${SOURCES} )
target_link_libraries( ${PROJECT_NAME} zypp-allsym zypp-tui )
