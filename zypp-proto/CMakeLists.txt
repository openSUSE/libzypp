INCLUDE_DIRECTORIES ( ${LIBZYPP_SOURCE_DIR} )

SET( zypp_proto_HEADERS
  target/CommitMessages.h
)

SET( zypp_proto_SOURCES
  target/CommitMessages.cc
)

ADD_LIBRARY( zypp-protobuf STATIC
  core/envelope.proto media/provider.proto test/tvm.proto ${zypp_proto_HEADERS} ${zypp_proto_SOURCES}
)

protobuf_generate( TARGET zypp-protobuf )

target_include_directories ( zypp-protobuf PRIVATE ${CMAKE_CURRENT_BINARY_DIR} )

target_link_libraries( zypp-protobuf zypp-core )
target_link_libraries( zypp-protobuf protobuf::libprotobuf-lite )
