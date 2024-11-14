ADD_TESTS(
  EventLoop
  IOBuffer
  ResourceLocks
  UnixSignalSource
  Pipelines
)

ADD_SUBDIRECTORY( media )
ADD_SUBDIRECTORY( io )
ADD_SUBDIRECTORY( rpc )

#add_executable( apitest NgAPI_test.cc )
#target_include_directories( apitest PUBLIC ${LIBZYPP_SOURCE_DIR} ${LIBZYPP_BINARY_DIR} ${LIBGLIB_INCLUDE_DIRS} )
#target_link_libraries( apitest zypp-glib )
