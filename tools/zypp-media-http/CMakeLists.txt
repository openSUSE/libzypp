PROJECT( zypp-media-http C CXX )

set (CMAKE_CXX_STANDARD 17)
SET (CMAKE_CXX_EXTENSIONS OFF)

INCLUDE_DIRECTORIES ( ${zypp-media-http_SOURCE_DIR} )
include(downloader/CMakeLists.txt)

SET( SOURCES
  main.cc
  networkprovider.cc
  networkprovider.h
  ${downloader_HEADERS}
  ${downloader_SRCS}
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
target_link_libraries( ${PROJECT_NAME} zypp-curl )
target_link_libraries( ${PROJECT_NAME} zypp-media )
target_link_libraries( ${PROJECT_NAME} zypp-core )

IF ( INSTALL_NG_BINARIES )
  INSTALL( TARGETS ${PROJECT_NAME} DESTINATION "${ZYPP_LIBEXEC_INSTALL_DIR}/workers" )
ENDIF()

set_target_properties( ${PROJECT_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${LIBZYPP_BINARY_DIR}/tools/workers"
    LIBRARY_OUTPUT_DIRECTORY "${LIBZYPP_BINARY_DIR}/tools/workers"
    RUNTIME_OUTPUT_DIRECTORY "${LIBZYPP_BINARY_DIR}/tools/workers"
)
