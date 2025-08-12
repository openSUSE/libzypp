ADD_DEFINITIONS( -DZYPP_DLL )

SET( zypp_media_detail_HEADERS
  media/detail/DownloadProgressTracker.h
  media/detail/MediaNetworkRequestExecutor.h
  media/detail/OptionalDownloadProgressReport.h
)

SET( zypp_media_SRCS
  media/detail/DownloadProgressTracker.cc
  media/detail/MediaNetworkRequestExecutor.cc
  media/detail/OptionalDownloadProgressReport.cc
  media/MediaHandler.cc
  media/MediaHandlerFactory.cc
  media/MediaNFS.cc
  media/MediaCD.cc
  media/MediaDIR.cc
  media/MediaDISK.cc
  media/MediaCIFS.cc
  media/MediaNetworkCommonHandler.cc
  media/MediaCurl.cc
  media/MediaCurl2.cc
  media/MediaISO.cc
  media/MediaPlugin.cc
  media/MediaSource.cc
  media/MediaManager.cc
  media/MediaPriority.cc
  media/UrlResolverPlugin.cc
)

SET( zypp_media_HEADERS
  media/MediaCD.h
  media/MediaCIFS.h
  media/MediaCurl.h
  media/MediaCurl2.h
  media/MediaNetworkCommonHandler.h
  media/MediaDIR.h
  media/MediaDISK.h
  media/MediaHandler.h
  media/MediaHandlerFactory.h
  media/MediaISO.h
  media/MediaPlugin.h
  media/MediaManager.h
  media/MediaNFS.h
  media/MediaSource.h
  media/MediaPriority.h
  media/UrlResolverPlugin.h
)

INSTALL(  FILES
  ${zypp_media_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/media
)



include(${zypp-libs_SOURCE_DIR}/zypp-logic/zypp/zypp.cmake)
SET( zypp_HEADER
  ${zypp_media_HEADERS}
  ${zypp_media_detail_HEADERS}
  ng/context.h
  ng/media/provide.h
)

SET( zypp_SOURCES
  ${zypp_media_SRCS}
  ng/context.cc
  ng/media/provide.cc
)

zypp_add_zypp_target( TARGETNAME zypp-objlib HEADERS ${zypp_HEADER} SOURCES ${zypp_SOURCES} )

# due to limitations in cmake object libraries feature( object files are not derived transiently, they are only added to the target if they are added DIRECTLY)
# we have this function that adds all zypp objects to a specified target.
function(add_zypp_lib target shared_or_static )

  set ( _link_mode PRIVATE )
  if ( ${shared_or_static} STREQUAL STATIC )
    message(NOTICE "Building ${target} static")
    set ( _link_mode INTERFACE )
  else()
    message(NOTICE "Building ${target} shared")
  endif()

  ADD_LIBRARY( ${target} ${shared_or_static} )

  SET_TARGET_PROPERTIES ( ${target} PROPERTIES VERSION "${LIBZYPP_VERSION_INFO}" )
  SET_TARGET_PROPERTIES ( ${target} PROPERTIES SOVERSION "${LIBZYPP_SOVERSION_INFO}" )

  target_link_libraries( ${target} PRIVATE zypp_lib_compiler_flags )
  target_link_libraries( ${target} PRIVATE zypp-objlib )
  target_link_libraries( ${target} PRIVATE commit-proto-obj )

  # CAUTION, link those libs always first and always in that order,
  # otherwise you get symbol lookup errors when linking against libzypp
  target_link_libraries( ${target} ${_link_mode} zypp-common )
  target_link_libraries( ${target} ${_link_mode} zypp-curl )
  target_link_libraries( ${target} ${_link_mode} zypp-media )


  # CAUTION, link zypp-tui.a at last, to prevent undefined references
  # in case tui functions are used in libzypp.
  target_link_libraries( ${target} ${_link_mode} zypp-tui )

  target_link_libraries( ${target} ${_link_mode} zypp-core )

  target_link_libraries( ${target} ${_link_mode} ${LibSolv_LIBRARIES} )
  target_link_libraries( ${target} ${_link_mode} ${RPM_LIBRARY} ${RPMIO_LIBRARY} )
  target_link_libraries( ${target} ${_link_mode} ${GETTEXT_LIBRARIES} )
  target_link_libraries( ${target} ${_link_mode} ${Boost_THREAD_LIBRARY})
  target_link_libraries( ${target} ${_link_mode} ${YAML_CPP_LIBRARIES})

  IF ( UDEV_FOUND )
    target_link_libraries( ${target} ${_link_mode} ${UDEV_LIBRARY} )
  ELSE ( UDEV_FOUND )
    IF ( HAL_FOUND )
      target_link_libraries( ${target} ${_link_mode} ${HAL_LIBRARY} ${HAL_STORAGE_LIBRARY} ${DBUS_LIBRARY} )
    ENDIF ( HAL_FOUND )
  ENDIF ( UDEV_FOUND )
endfunction()

add_zypp_lib( zypp SHARED )

target_include_directories( zypp  INTERFACE
  $<BUILD_INTERFACE:${LIBZYPP_BINARY_DIR}>
  $<BUILD_INTERFACE:${LIBZYPP_SOURCE_DIR}>
  $<BUILD_INTERFACE:${zypp-libs_SOURCE_DIR}/zypp-logic>
)

set_target_properties ( zypp PROPERTIES LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libzypp.map" )
INSTALL(TARGETS zypp LIBRARY DESTINATION ${LIB_INSTALL_DIR} )

# library with more symbols for tests etc
add_zypp_lib( zypp-allsym STATIC )

target_include_directories(  zypp-allsym  INTERFACE
  $<BUILD_INTERFACE:${LIBZYPP_BINARY_DIR}>
  $<BUILD_INTERFACE:${LIBZYPP_SOURCE_DIR}>
  $<BUILD_INTERFACE:${zypp-libs_SOURCE_DIR}/zypp-logic>
)

# install XML schemas
FILE( GLOB YUM_SCHEMA_FILES ${LIBZYPP_SOURCE_DIR}/zypp/parser/yum/schema/*.rng )
INSTALL(FILES ${YUM_SCHEMA_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/zypp/schema/yum" )

ADD_CUSTOM_TARGET ( buildmon
  COMMAND test ! -x ./buildmon || ./buildmon
)

ADD_CUSTOM_COMMAND (
  TARGET zypp POST_BUILD
  COMMAND test ! -x ./buildmon || ./buildmon
)
