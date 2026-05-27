PROJECT(zyppng-cxx)

OPTION( LIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT "Builtin default for zypp.conf:download.use_deltarpm" OFF )
OPTION( LIBZYPP_CONFIG_USE_CLASSIC_RPMTRANS_BY_DEFAULT "Use the CLASSIC_RPMTRANS install backend rather than SINGLE_RPMTRANS" OFF )
OPTION( LIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT "By default use the old CURL backend for downloads" OFF )
OPTION( LIBZYPP_CONFIG_USE_SERIAL_PACKAGE_DOWNLOAD_BY_DEFAULT "By default don't download packages in parallel" OFF )

SET( ZYPP_LOGIC_TARGET_PREFIX "zyppng" )
SET( ZYPP_LOGIC_INSTALL_CXX   false    )

add_subdirectory( shared )
add_subdirectory( zypp-core )
#add_subdirectory( zypp-common )
add_subdirectory( zypp-media )
add_subdirectory( zypp-curl )
add_subdirectory( zypp )


# ---------------------------------------------------------------------------
# zyppng — production static wrapper around zyppng-objlib.
#
# Linked exclusively by libzypp-glib. The C++ symbols carried here are not
# exposed to end users; only the GLib-C API surface is public. No version
# script is needed because there is no public C++ ABI.
#
# Test executables do NOT link this target. They link zyppng-objlib-test
# directly (an OBJECT library carrying the same module sources plus test
# impl units), which gives them full module-internal visibility.
#
# zyppng-objlib already propagates its system-dep INTERFACE links so this
# wrapper only needs to add itself + the commit-proto object library.
# ---------------------------------------------------------------------------
ADD_LIBRARY( zyppng STATIC zypp/dummy.cc )

SET_TARGET_PROPERTIES( zyppng PROPERTIES VERSION   "${LIBZYPP_VERSION_INFO}" )
SET_TARGET_PROPERTIES( zyppng PROPERTIES SOVERSION "${LIBZYPP_SOVERSION_INFO}" )

target_link_libraries( zyppng PRIVATE zyppng_lib_compiler_flags )
target_link_libraries( zyppng PUBLIC zyppng-objlib )
target_link_libraries( zyppng PRIVATE zyppng-commit-proto-obj )
