PROJECT( zypp-media-iso C CXX )

set (CMAKE_CXX_STANDARD 17)
SET (CMAKE_CXX_EXTENSIONS OFF)

SET( SOURCES
  main.cc
  isoprovider.cc
  isoprovider.h
  ${ZYPP_TOOLS_DIR}/zypp-media-dir/dirprovider.h
  ${ZYPP_TOOLS_DIR}/zypp-media-dir/dirprovider.cc
  ${ZYPP_TOOLS_DIR}/zypp-media-disk/diskprovider.h
  ${ZYPP_TOOLS_DIR}/zypp-media-disk/diskprovider.cc
  ${ZYPP_TOOLS_DIR}/zypp-media-nfs/nfsprovider.h
  ${ZYPP_TOOLS_DIR}/zypp-media-nfs/nfsprovider.cc
  ${ZYPP_TOOLS_DIR}/zypp-media-smb/smbprovider.h
  ${ZYPP_TOOLS_DIR}/zypp-media-smb/smbprovider.cc
)

include_directories(
  ${ZYPP_TOOLS_DIR}/zypp-media-dir
  ${ZYPP_TOOLS_DIR}/zypp-media-disk
  ${ZYPP_TOOLS_DIR}/zypp-media-nfs
  ${ZYPP_TOOLS_DIR}/zypp-media-smb
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
target_link_libraries( ${PROJECT_NAME} zyppng_exe_compiler_flags )
target_link_libraries( ${PROJECT_NAME} zyppng-media )
target_link_libraries( ${PROJECT_NAME} zyppng-core )

IF ( INSTALL_NG_BINARIES )
  INSTALL( TARGETS ${PROJECT_NAME} DESTINATION "${ZYPP_LIBEXEC_INSTALL_DIR}/workers" )
ENDIF()

set_target_properties( ${PROJECT_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${LIBZYPP_BINARY_DIR}/tools/workers"
    LIBRARY_OUTPUT_DIRECTORY "${LIBZYPP_BINARY_DIR}/tools/workers"
    RUNTIME_OUTPUT_DIRECTORY "${LIBZYPP_BINARY_DIR}/tools/workers"
)
