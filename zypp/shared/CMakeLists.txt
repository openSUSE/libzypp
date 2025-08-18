# This subproject contains small shared object libraries
# usually used in libzypp and workers, or for tests.

INCLUDE_DIRECTORIES ( ${LIBZYPP_SOURCE_DIR} ${LIBZYPP_BINARY_DIR} ${zypp-libs_SOURCE_DIR}/zypp-logic )
ADD_DEFINITIONS( -DLOCALEDIR="${CMAKE_INSTALL_PREFIX}/share/locale" -DTEXTDOMAIN="zypp" -DZYPP_DLL )

# rpm protocol definition, could be moved to a zypp-rpm lib some day
# if more than one class is required to be shared between libzypp and zypp-rpm
ADD_LIBRARY( commit-proto-obj OBJECT commit/CommitMessages.h commit/CommitMessages.cc )
target_link_libraries( commit-proto-obj PRIVATE zypp_lib_compiler_flags )

# tvm protocol lib
ADD_LIBRARY( tvm-protocol-obj OBJECT tvm/tvmsettings.h tvm/tvmsettings.cc )
target_link_libraries( tvm-protocol-obj PRIVATE zypp_lib_compiler_flags )
