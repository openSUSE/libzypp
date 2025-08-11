## ############################################################

FILE( GLOB ALLCC RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cc" )
STRING( REPLACE ".cc" ";" APLLPROG ${ALLCC} )

# make sure not to statically linked installed tools
SET( LINKALLSYM CalculateReusableBlocks DownloadFiles zypp-runpurge)

FOREACH( loop_var ${APLLPROG} )
  ADD_EXECUTABLE( ${loop_var}
    ${loop_var}.cc
  )
  TARGET_LINK_LIBRARIES( ${loop_var} PUBLIC zypp_exe_compiler_flags ${Boost_PROGRAM_OPTIONS_LIBRARY} )
  LIST( FIND LINKALLSYM ${loop_var} NEEDALLSYM )
  if ( NEEDALLSYM EQUAL -1 )
    TARGET_LINK_LIBRARIES( ${loop_var} PUBLIC zypp )
  ELSE()
    TARGET_LINK_LIBRARIES( ${loop_var} PUBLIC zypp-allsym )
  ENDIF()
ENDFOREACH( loop_var )

## ############################################################

INSTALL(TARGETS zypp-NameReqPrv		DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")


SET( ZYPP_TOOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR} )

ADD_SUBDIRECTORY( zypp-rpm )
ADD_SUBDIRECTORY( zypp-media-http )
ADD_SUBDIRECTORY( zypp-media-disc )
ADD_SUBDIRECTORY( zypp-media-nfs )
ADD_SUBDIRECTORY( zypp-media-smb )
ADD_SUBDIRECTORY( zypp-media-dir )
ADD_SUBDIRECTORY( zypp-media-disk )
ADD_SUBDIRECTORY( zypp-media-iso )
ADD_SUBDIRECTORY( zypp-media-copy )
ADD_SUBDIRECTORY( zypp-media-tvm )
ADD_SUBDIRECTORY( zypp-media-chksum )

## ############################################################
OPTION( ENABLE_DEVTOOL_REPOMIRROR "Devtool: build repomirror?" OFF )
IF ( ENABLE_DEVTOOL_REPOMIRROR )
    MESSAGE( STATUS "DEVTOOL_REPOMIRROR enabled" )
    FIND_PACKAGE( Notcurses++ QUIET )
    if ( NOT Notcurses++_FOUND )
          MESSAGE( FATAL_ERROR " DEVTOOL_REPOMIRROR: Notcurses++ not found" )
    ENDIF()
    ADD_SUBDIRECTORY( repomirror )
ENDIF ( ENABLE_DEVTOOL_REPOMIRROR )
## ############################################################

## ############################################################
OPTION( ENABLE_DEVTOOL_REFRESHNG "Devtool: build refreshng?" OFF )
IF ( ENABLE_DEVTOOL_REFRESHNG )
  ADD_SUBDIRECTORY( refresh-ng )
ENDIF ( ENABLE_DEVTOOL_REFRESHNG )

## ############################################################
