find_program( NGINX nginx )
find_program( VSFTPD vsftpd )
find_library( LIBFCGI NAMES fcgi )
find_library( LIBFCGI++ NAMES fcgi++ )

if ( NOT NGINX )
  message( FATAL_ERROR "nginx not found" )
else()
  message ( "Using nginx binary at: ${NGINX}" )
  ADD_DEFINITIONS( -DWEBSRV_BINARY="${NGINX}" )
endif()

if ( NOT VSFTPD )
  message( FATAL_ERROR "vsftpd not found" )
else()
  message ( "Using vsftpd binary at: ${VSFTPD}" )
  ADD_DEFINITIONS( -DFTPSRV_BINARY="${VSFTPD}" )
endif()

if ( NOT LIBFCGI )
  message( FATAL_ERROR "libfcgi not found" )
else()
  message ( "Using libfcgi library at: ${LIBFCGI}" )
endif()

if ( NOT LIBFCGI++ )
  message( FATAL_ERROR "libfcgi++ not found" )
else()
  message ( "Using libfcgi++ library at: ${LIBFCGI++}" )
endif()

add_definitions( -DSRCDIR="${CMAKE_CURRENT_SOURCE_DIR}")

ADD_LIBRARY(zypp_test_utils STATIC
 FtpServer.h
 FtpServer.cc
 TestSetup.h
 TestTools.h
 WebServer.h
 WebServer.cc
)

# These headers are in different subdirectories depending on the
# distribution
CHECK_INCLUDE_FILE( fastcgi/fcgiapp.h FASTCGI_HEADER_DIR )
IF ( FASTCGI_HEADER_DIR )
  target_compile_definitions( zypp_test_utils PRIVATE FCGI_IN_SUBDIR=1)
ENDIF ( FASTCGI_HEADER_DIR )

target_link_libraries( zypp_test_utils PRIVATE zypp_lib_compiler_flags )
TARGET_LINK_LIBRARIES( zypp_test_utils PUBLIC ${LIBFCGI} ${LIBFCGI++} pthread )
