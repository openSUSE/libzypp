include(GNUInstallDirs)

# Library
IF ( DEFINED LIB )
  SET ( LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${LIB}" )
ELSE ( DEFINED  LIB )
  SET ( LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" )
ENDIF ( DEFINED  LIB )
MESSAGE(STATUS "Libraries will be installed in ${LIB_INSTALL_DIR}" )
# Headers
IF (DEFINED INCLUDE)
  SET (INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${INCLUDE}")
else (DEFINED INCLUDE)
  SET (INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include")
ENDIF (DEFINED  INCLUDE)
MESSAGE (STATUS "Header files will be installed in ${INCLUDE_INSTALL_DIR}")

# system configuration dir (etc)
IF( NOT DEFINED SYSCONFDIR )
  IF ( ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr" )
    # if installing in usr, set sysconfg to etc
    SET( SYSCONFDIR /etc )
  ELSE ( ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr" )
    SET ( SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/etc" )
  ENDIF ( ${CMAKE_INSTALL_PREFIX} STREQUAL "/usr" )
ENDIF( NOT DEFINED SYSCONFDIR )
MESSAGE(STATUS "Config files will be installed in ${SYSCONFDIR}" )

# install directory for private executables that are not for the user
SET ( ZYPP_LIBEXEC_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/zypp" )

# usr INSTALL_PREFIX
IF( DEFINED CMAKE_INSTALL_PREFIX )
  SET( INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} )
ELSE( DEFINED CMAKE_INSTALL_PREFIX )
  SET( INSTALL_PREFIX /usr )
ENDIF( DEFINED CMAKE_INSTALL_PREFIX )

# system configuration dir (etc)
IF( NOT DEFINED MANDIR )
  SET( MANDIR ${INSTALL_PREFIX}/share/man )
ENDIF( NOT DEFINED MANDIR )
MESSAGE( "** Manual files will be installed in ${MANDIR}" )

####################################################################
# CONFIGURATION                                                    #
####################################################################

IF( NOT DEFINED DOC_INSTALL_DIR )
  SET( DOC_INSTALL_DIR
     "${CMAKE_INSTALL_PREFIX}/share/doc/packages/${PACKAGE}"
     CACHE PATH "The install dir for documentation (default prefix/share/doc/packages/${PACKAGE})"
     FORCE
  )
ENDIF( NOT DEFINED DOC_INSTALL_DIR )

####################################################################
# INCLUDES                                                         #
####################################################################

SET( ZYPPCOMMON_CXX_STANDARD 17 )
#SET (CMAKE_INCLUDE_DIRECTORIES_BEFORE ON)
INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} SYSTEM )

####################################################################
# RPM SPEC                                                         #
####################################################################

function( SPECFILE_EXT )

endfunction()

MACRO(SPECFILE)
  MESSAGE(STATUS "Writing spec file...")
  CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/${PACKAGE}.spec.cmake ${PROJECT_BINARY_DIR}/package/${PACKAGE}.spec @ONLY)
  MESSAGE(STATUS "I hate you rpm-lint...!!!")
  IF (EXISTS ${PROJECT_SOURCE_DIR}/package/${PACKAGE}-rpmlint.cmake)
    CONFIGURE_FILE(${PROJECT_SOURCE_DIR}/package/${PACKAGE}-rpmlint.cmake ${PROJECT_BINARY_DIR}/package/${PACKAGE}-rpmlintrc @ONLY)
  ENDIF (EXISTS ${PROJECT_SOURCE_DIR}/package/${PACKAGE}-rpmlint.cmake)
ENDMACRO(SPECFILE)

####################################################################
# INSTALL                                                          #
####################################################################

MACRO(GENERATE_PACKAGING PACKAGE VERSION)

  # The following components are regex's to match anywhere (unless anchored)
  # in absolute path + filename to find files or directories to be excluded
  # from source tarball.
  SET (CPACK_SOURCE_IGNORE_FILES
  # hidden files
  "/\\\\..+$"
  # temporary files
  "\\\\.swp$"
  # backup files
  "~$"
  # others
  "\\\\.#"
  "/#"
  "/build/"
  "/_build/"
  # used before
  "/CVS/"
  "\\\\.o$"
  "\\\\.lo$"
  "\\\\.la$"
  "Makefile\\\\.in$"
  )

  SET(CPACK_PACKAGE_VENDOR "SUSE LLC")
  SET( CPACK_GENERATOR "TBZ2")
  SET( CPACK_SOURCE_GENERATOR "TBZ2")
  SET( CPACK_SOURCE_PACKAGE_FILE_NAME "${PACKAGE}-${VERSION}" )
  INCLUDE(CPack)

  SPECFILE()

  if ( ZYPP_STACK_BUILD )
    set( target_prefix "${PROJECT_NAME}_" )
  endif()

  ADD_CUSTOM_TARGET( ${target_prefix}svncheck
    COMMAND cd ${PROJECT_SOURCE_DIR} && LC_ALL=C git status | grep -q "nothing to commit .working directory clean."
  )

  SET( AUTOBUILD_COMMAND
    COMMAND ${CMAKE_COMMAND} -E remove ${PROJECT_BINARY_DIR}/package/*.tar.bz2
    COMMAND ${CMAKE_MAKE_PROGRAM} package_source
    COMMAND ${CMAKE_COMMAND} -E copy ${CPACK_SOURCE_PACKAGE_FILE_NAME}.tar.bz2 ${PROJECT_BINARY_DIR}/package
    COMMAND ${CMAKE_COMMAND} -E remove ${CPACK_SOURCE_PACKAGE_FILE_NAME}.tar.bz2
    COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_SOURCE_DIR}/package/${PACKAGE}.changes" "${PROJECT_BINARY_DIR}/package/${PACKAGE}.changes"
  )

  ADD_CUSTOM_TARGET( ${target_prefix}srcpackage_local
    ${AUTOBUILD_COMMAND}
  )

  ADD_CUSTOM_TARGET( ${target_prefix}srcpackage
    COMMAND ${CMAKE_MAKE_PROGRAM} ${target_prefix}svncheck
    ${AUTOBUILD_COMMAND}
  )
ENDMACRO(GENERATE_PACKAGING)


function( GENERATE_PACKAGING_EXT )

endfunction()
