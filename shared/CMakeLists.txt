# This subproject contains small shared object libraries
# usually used in libzypp and workers, or for tests.

# rpm protocol definition, could be moved to a zypp-rpm lib some day
# if more than one class is required to be shared between libzypp and zypp-rpm
ADD_LIBRARY( commit-proto-obj OBJECT commit/CommitMessages.h commit/CommitMessages.cc )

# tvm protobuf protocol lib
ADD_LIBRARY( tvm-protobuf-obj OBJECT tvm/tvm.proto tvm/zerocopystreams.cc )
target_include_directories ( tvm-protobuf-obj PRIVATE ${CMAKE_CURRENT_BINARY_DIR} )
protobuf_generate( TARGET tvm-protobuf-obj )

