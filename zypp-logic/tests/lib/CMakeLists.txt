add_definitions( -DSRCDIR="${CMAKE_CURRENT_SOURCE_DIR}")

if( NOT DISABLE_MEDIABACKEND_TESTS )
  find_program( NGINX nginx )
  find_program( VSFTPD vsftpd )
  find_program( SQUIDPROXY squid )
  find_program( SQUIDPROXY_AUTH basic_ncsa_auth HINTS ${CMAKE_INSTALL_BINDIR} ${CMAKE_INSTALL_SBINDIR} ${CMAKE_INSTALL_LIBEXECDIR} "/usr/libexec" "/usr/lib" PATH_SUFFIXES squid)
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

  if ( NOT SQUIDPROXY )
    message( FATAL_ERROR "squid not found" )
  else()
    message ( "Using squid binary at: ${SQUIDPROXY}" )
    ADD_DEFINITIONS( -DPROXYSRV_BINARY="${SQUIDPROXY}" )
  endif()

  if ( NOT SQUIDPROXY_AUTH )
    message( FATAL_ERROR "basic_ncsa_auth not found" )
  else()
    message ( "Using basic_ncsa_auth binary at: ${SQUIDPROXY_AUTH}" )
    ADD_DEFINITIONS( -DPROXYAUTH_BINARY="${SQUIDPROXY_AUTH}" )
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

  set( MEDIA_SOURCES
    FtpServer.h
    FtpServer.cc
    ProxyServer.h
    ProxyServer.cc
    WebServer.h
    WebServer.cc
  )

endif()

ADD_LIBRARY(${TESTLIBNAME} STATIC
 TestSetup.h
 TestTools.h
 TestTools.cc
 ${MEDIA_SOURCES}
)

target_link_libraries( ${TESTLIBNAME} PRIVATE ${ZYPP_COMP_FLAGS} )

if( NOT DISABLE_MEDIABACKEND_TESTS )

  # These headers are in different subdirectories depending on the
  # distribution
  CHECK_INCLUDE_FILE( fastcgi/fcgiapp.h FASTCGI_HEADER_DIR )
  IF ( FASTCGI_HEADER_DIR )
    target_compile_definitions( ${TESTLIBNAME} PRIVATE FCGI_IN_SUBDIR=1)
  ENDIF ( FASTCGI_HEADER_DIR )

  TARGET_LINK_LIBRARIES( ${TESTLIBNAME} PUBLIC ${LIBFCGI} ${LIBFCGI++} pthread )
endif()

