ADD_DEFINITIONS( -DZYPP_DLL )


# start of code that is still a candidate for being moved to zypp-logic


SET( zypp_SRCS
  Application.cc
  Capabilities.cc
  Capability.cc
  Changelog.cc
  Digest.cc
  DiskUsageCounter.cc
  DownloadMode.cc
  PluginScript.cc
  PluginScriptException.cc
  PluginExecutor.cc
  Fetcher.cc
  FileChecker.cc
  Glob.cc
  HistoryLog.cc
  HistoryLogData.cc
  InstanceId.cc
  KeyRing.cc
  KeyRingContexts.cc
  Locks.cc
  MediaSetAccess.cc
  Package.cc
  Patch.cc
  PathInfo.cc
  Pattern.cc
  PoolItem.cc
  PoolItemBest.cc
  PoolQuery.cc
  PoolQueryResult.cc
  ProblemSolution.cc
  Product.cc
  ProvideFilePolicy.cc
  PurgeKernels.cc
  RepoInfo.cc
  RepoManager.cc
  RepoManagerOptions.cc
  Repository.cc
  RepoStatus.cc
  ResObject.cc
  Resolvable.cc
  Resolver.cc
  ResolverFocus.cc
  ResolverProblem.cc
  ResPool.cc
  ResPoolProxy.cc
  ResStatus.cc
  ServiceInfo.cc
  Signature.cc
  SrcPackage.cc
  SysContent.cc
  Target.cc
  TmpPath.cc
  VendorAttr.cc
  VendorSupportOptions.cc
  ZYpp.cc
  ZYppCallbacks.cc
  ZYppCommitPolicy.cc
  ZYppCommitResult.cc
  ZYppFactory.cc
)
SET( zypp_EARLY_SRCS
  ZConfig.cc
)

SET( zypp_HEADERS
  Application.h
  Bitmap.h
  Callback.h
  Capabilities.h
  Capability.h
  Changelog.h
  CheckSum.h
  Digest.h
  DiskUsageCounter.h
  DownloadMode.h
  PluginScript.h
  PluginScriptException.h
  PluginExecutor.h
  Fetcher.h
  FileChecker.h
  Glob.h
  HistoryLog.h
  HistoryLogData.h
  InstanceId.h
  KeyContext.h
  KeyRing.h
  KeyRingContexts.h
  Locks.h
  ManagedFile.h
  MediaProducts.h
  MediaSetAccess.h
  Vendor.h
  Package.h
  PackageKeyword.h
  Patch.h
  PathInfo.h
  Pattern.h
  PoolItem.h
  PoolItemBest.h
  PoolQuery.h
  PoolQueryUtil.tcc
  PoolQueryResult.h
  ProblemSolution.h
  ProblemTypes.h
  Product.h
  ProvideFilePolicy.h
  PurgeKernels.h
  RepoInfo.h
  RepoManager.h
  RepoManagerFlags.h
  RepoManagerOptions.h
  Repository.h
  RepoStatus.h
  Filter.h
  ResFilters.h
  ResObject.h
  ResObjects.h
  Resolvable.h
  Resolver.h
  ResolverFocus.h
  ResolverProblem.h
  ResPool.h
  ResPoolProxy.h
  ResStatus.h
  ServiceInfo.h
  Signature.h
  SrcPackage.h
  SysContent.h
  Target.h
  TmpPath.h
  VendorAttr.h
  VendorSupportOptions.h
  ZConfig.h
  ZYppCallbacks.h
  ZYppCommit.h
  ZYppCommitPolicy.h
  ZYppCommitResult.h
  ZYppFactory.h
  ZYpp.h
)

INSTALL(  FILES ${zypp_HEADERS} DESTINATION "${INCLUDE_INSTALL_DIR}/zypp" )

SET( zypp_base_SRCS
  base/Backtrace.cc
  base/Random.cc
  base/Measure.cc
)

SET( zypp_base_HEADERS
  base/Backtrace.h
  base/Collector.h
  base/Random.h
  base/Algorithm.h
  base/Counter.h
  base/Debug.h
  base/Json.h
  base/LocaleGuard.h
  base/Measure.h
  base/NamedValue.h
  base/ReferenceCounted.h
  base/Signal.h
  base/ValueTransform.h
)

INSTALL(  FILES
  ${zypp_base_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/base
)

SET( zypp_ng_SRCS
  ng/progressobserveradaptor.cc
  ng/reporthelper.cc
  ng/repomanager.cc
  ng/userrequest.cc
  ng/repo/downloader.cc
  ng/repo/refresh.cc
  ng/repo/workflows/plaindir.cc
  ng/repo/workflows/repodownloaderwf.cc
  ng/repo/workflows/repomanagerwf.cc
  ng/repo/workflows/rpmmd.cc
  ng/repo/workflows/serviceswf.cc
  ng/repo/workflows/susetags.cc
  ng/workflows/checksumwf.cc
  ng/workflows/downloadwf.cc
  ng/workflows/keyringwf.cc
  ng/workflows/repoinfowf.cc
  ng/workflows/signaturecheckwf.cc
)

SET( zypp_ng_HEADERS
  ng/progressobserveradaptor.h
  ng/reporthelper.h
  ng/repomanager.h
  ng/RepoManager
  ng/userrequest.h
  ng/UserRequest
  ng/repo/downloader.h
  ng/repo/Downloader
  ng/repo/refresh.h
  ng/repo/Refresh
  ng/repo/workflows/plaindir.h
  ng/repo/workflows/repodownloaderwf.h
  ng/repo/workflows/repomanagerwf.h
  ng/repo/workflows/rpmmd.h
  ng/repo/workflows/serviceswf.h
  ng/repo/workflows/susetags.h
  ng/workflows/checksumwf.h
  ng/workflows/downloadwf.h
  ng/workflows/keyringwf.h
  ng/workflows/repoinfowf.h
  ng/workflows/signaturecheckwf.h
)

SET( zypp_ng_private_HEADERS
  ng/private/repomanager_p.h
)

#INSTALL( FILES
#  ${zypp_ng_HEADERS}
#  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/repo
#)


SET( zypp_parser_SRCS
  parser/HistoryLogReader.cc
  parser/RepoFileReader.cc
  parser/RepoindexFileReader.cc
  parser/ServiceFileReader.cc
  parser/ProductFileReader.cc
)

SET( zypp_parser_HEADERS
  parser/HistoryLogReader.h
  parser/RepoFileReader.h
  parser/RepoindexFileReader.h
  parser/ServiceFileReader.h
  parser/ProductFileReader.h
)

INSTALL(  FILES
  ${zypp_parser_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/parser
)

SET( zypp_parser_susetags_SRCS
  parser/susetags/RepoIndex.cc
  parser/susetags/ContentFileReader.cc
)

SET( zypp_parser_susetags_HEADERS
  parser/susetags/RepoIndex.h
  parser/susetags/ContentFileReader.h
)

INSTALL(  FILES
  ${zypp_parser_susetags_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/parser/susetags
)

SET( zypp_parser_xml_SRCS
  parser/xml/Node.cc
  parser/xml/ParseDef.cc
  parser/xml/ParseDefConsume.cc
  parser/xml/ParseDefException.cc
  parser/xml/Reader.cc
  parser/xml/XmlString.cc
  parser/xml/libxmlfwd.cc
)

SET( zypp_parser_xml_HEADERS
  parser/xml/Parse.h
  parser/xml/Node.h
  parser/xml/ParseDef.h
  parser/xml/ParseDefConsume.h
  parser/xml/ParseDefException.h
  parser/xml/ParseDefTraits.h
  parser/xml/Reader.h
  parser/xml/XmlString.h
  parser/xml/libxmlfwd.h
)

INSTALL(  FILES
  ${zypp_parser_xml_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/parser/xml
)

SET( zypp_parser_yum_SRCS
  parser/yum/RepomdFileReader.cc
)

SET( zypp_parser_yum_HEADERS
  parser/yum/RepomdFileReader.h
)

INSTALL(  FILES
  ${zypp_parser_yum_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/parser/yum
)

SET( zypp_pool_SRCS
  pool/PoolImpl.cc
  pool/PoolStats.cc
)

SET( zypp_pool_HEADERS
  pool/PoolImpl.h
  pool/PoolStats.h
  pool/PoolTraits.h
  pool/ByIdent.h
)

INSTALL(  FILES
  ${zypp_pool_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/pool
)

SET( zypp_sat_SRCS
  sat/Pool.cc
  sat/Solvable.cc
  sat/SolvableSet.cc
  sat/SolvIterMixin.cc
  sat/SolvableSpec.cc
  sat/Map.cc
  sat/FileConflicts.cc
  sat/Transaction.cc
  sat/WhatProvides.cc
  sat/WhatObsoletes.cc
  sat/LocaleSupport.cc
  sat/LookupAttr.cc
  sat/Queue.cc
)

SET( zypp_sat_HEADERS
  sat/Pool.h
  sat/Solvable.h
  sat/SolvableSet.h
  sat/SolvableSpec.h
  sat/SolvableType.h
  sat/SolvIterMixin.h
  sat/Map.h
  sat/FileConflicts.h
  sat/Transaction.h
  sat/WhatProvides.h
  sat/WhatObsoletes.h
  sat/LocaleSupport.h
  sat/LookupAttr.h
  sat/LookupAttrTools.h
  sat/Queue.h
)

INSTALL(  FILES
  ${zypp_sat_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/sat
)

SET( zypp_sat_detail_SRCS
  sat/detail/PoolImpl.cc
)

SET( zypp_sat_detail_HEADERS
  sat/detail/PoolImpl.h
)

INSTALL(  FILES
  ${zypp_sat_detail_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/sat/detail
)



# SET( zypp_solver_detail_SRCS )

SET( zypp_solver_HEADERS
  solver/Types.h
)

INSTALL(  FILES
  ${zypp_solver_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/solver
)

SET( zypp_solver_detail_SRCS
  solver/detail/ProblemSolutionIgnore.cc
  solver/detail/ProblemSolutionCombi.cc
  solver/detail/Resolver.cc
  solver/detail/SolutionAction.cc
  solver/detail/Testcase.cc
  solver/detail/SolverQueueItem.cc
  solver/detail/SolverQueueItemInstall.cc
  solver/detail/SolverQueueItemDelete.cc
  solver/detail/SolverQueueItemUpdate.cc
  solver/detail/SolverQueueItemInstallOneOf.cc
  solver/detail/SolverQueueItemLock.cc
  solver/detail/SATResolver.cc
  solver/detail/SystemCheck.cc
)

SET( zypp_solver_detail_HEADERS
  solver/detail/ProblemSolutionIgnore.h
  solver/detail/ProblemSolutionCombi.h
  solver/detail/Resolver.h
  solver/detail/SolutionAction.h
  solver/detail/Testcase.h
  solver/detail/Types.h
  solver/detail/SolverQueueItem.h
  solver/detail/SolverQueueItemInstall.h
  solver/detail/SolverQueueItemDelete.h
  solver/detail/SolverQueueItemUpdate.h
  solver/detail/SolverQueueItemInstallOneOf.h
  solver/detail/SolverQueueItemLock.h
  solver/detail/ItemCapKind.h
  solver/detail/SATResolver.h
  solver/detail/SystemCheck.h
)

INSTALL(  FILES
  ${zypp_solver_detail_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/solver/detail
)

SET( zypp_target_SRCS
  target/RpmPostTransCollector.cc
  target/RequestedLocalesFile.cc
  target/SolvIdentFile.cc
  target/HardLocksFile.cc
  target/commitpackagepreloader.cc
  target/CommitPackageCache.cc
  target/CommitPackageCacheImpl.cc
  target/CommitPackageCacheReadAhead.cc
  target/TargetCallbackReceiver.cc
  target/TargetException.cc
  target/TargetImpl.cc
  target/TargetImpl.commitFindFileConflicts.cc

)

SET( zypp_target_HEADERS
  target/RpmPostTransCollector.h
  target/RequestedLocalesFile.h
  target/SolvIdentFile.h
  target/HardLocksFile.h
  target/CommitPackageCache.h
  target/CommitPackageCacheImpl.h
  target/CommitPackageCacheReadAhead.h
  target/TargetCallbackReceiver.h
  target/TargetException.h
  target/TargetImpl.h
)


SET( zypp_target_detail_HEADERS
  target/private/commitpackagepreloader_p.h
)

INSTALL(  FILES
  ${zypp_target_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/target
)

SET( zypp_target_modalias_SRCS
  target/modalias/Modalias.cc
)

SET( zypp_target_modalias_HEADERS
  target/modalias/Modalias.h
)

INSTALL(  FILES
  ${zypp_target_modalias_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/target/modalias
)

SET( zypp_target_rpm_SRCS
  target/rpm/BinHeader.cc
  target/rpm/RpmCallbacks.cc
  target/rpm/RpmDb.cc
  target/rpm/RpmException.cc
  target/rpm/RpmHeader.cc
  target/rpm/librpmDb.cc
)

SET( zypp_target_rpm_HEADERS
  target/rpm/BinHeader.h
  target/rpm/RpmCallbacks.h
  target/rpm/RpmFlags.h
  target/rpm/RpmDb.h
  target/rpm/RpmException.h
  target/rpm/RpmHeader.h
  target/rpm/librpm.h
  target/rpm/librpmDb.h
)

INSTALL(  FILES
  ${zypp_target_rpm_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/target/rpm
)

SET( zypp_ui_SRCS
  ui/Selectable.cc
  ui/SelectableImpl.cc
  ui/Status.cc
  ui/UserWantedPackages.cc
)

SET( zypp_ui_HEADERS
  ui/SelFilters.h
  ui/Selectable.h
  ui/SelectableImpl.h
  ui/SelectableTraits.h
  ui/Status.h
  ui/UserWantedPackages.h
)

INSTALL(  FILES
  ${zypp_ui_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/ui
)

INSTALL(  FILES
  ${zypp_url_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/url
)

SET( zypp_zypp_detail_SRCS
  zypp_detail/ZYppImpl.cc
)

SET( zypp_zypp_detail_HEADERS
  zypp_detail/keyring_p.h
  zypp_detail/urlcredentialextractor_p.h
  zypp_detail/ZYppImpl.h
  zypp_detail/ZYppReadOnlyHack.h
)

INSTALL(  FILES
  ${zypp_zypp_detail_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/zypp_detail
)

SET( zypp_repo_SRCS
  repo/RepoException.cc
  repo/RepoMirrorList.cc
  repo/RepoType.cc
  repo/ServiceType.cc
  repo/PackageProvider.cc
  repo/SrcPackageProvider.cc
  repo/RepoProvideFile.cc
  repo/DeltaCandidates.cc
  repo/Applydeltarpm.cc
  repo/PackageDelta.cc
  repo/SUSEMediaVerifier.cc
  repo/RepoVariables.cc
  repo/RepoInfoBase.cc
  repo/PluginRepoverification.cc
  repo/PluginServices.cc
)

SET( zypp_repo_HEADERS
  repo/RepoException.h
  repo/RepoMirrorList.h
  repo/RepoType.h
  repo/ServiceType.h
  repo/PackageProvider.h
  repo/SrcPackageProvider.h
  repo/RepoProvideFile.h
  repo/DeltaCandidates.h
  repo/Applydeltarpm.h
  repo/PackageDelta.h
  repo/SUSEMediaVerifier.h
  repo/RepoVariables.h
  repo/RepoInfoBase.h
  repo/PluginRepoverification.h
  repo/PluginServices.h
)

INSTALL( FILES
  ${zypp_repo_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/repo
)

SET( zypp_repo_yum_SRCS
  repo/yum/RepomdFileCollector.cc
)

SET( zypp_repo_yum_HEADERS
  repo/yum/RepomdFileCollector.h
)

SET( zypp_repo_susetags_SRCS
)

SET( zypp_repo_susetags_HEADERS
)


####################################################################

SET( zypp_misc_HEADERS
  Misc.h
  misc/DefaultLoadSystem.h
  misc/CheckAccessDeleted.h
  misc/TestcaseSetup.h
  misc/LoadTestcase.h
)

SET( zypp_misc_SRCS
  misc/DefaultLoadSystem.cc
  misc/CheckAccessDeleted.cc
  misc/TestcaseSetup.cc
  misc/LoadTestcase.cc
)

INSTALL( FILES
  ${zypp_misc_HEADERS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/misc
)

####################################################################


# end of code that is still a candidate for being moved to zypp-logic


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

  ##########################

  ${zypp_target_rpm_HEADERS}
  ${zypp_parser_tagfile_HEADERS}
  ${zypp_parser_susetags_HEADERS}
  ${zypp_parser_yum_HEADERS}
  ${zypp_parser_xml_HEADERS}
  ${zypp_parser_HEADERS}
  ${zypp_ui_HEADERS}
  ${zypp_media_compat_proxyinfo_HEADERS}
  ${zypp_ng_HEADERS}
  ${zypp_ng_private_HEADERS}
  ${zypp_base_HEADERS}
  ${zypp_sat_HEADERS}
  ${zypp_sat_detail_HEADERS}
  ${zypp_solver_HEADERS}
  ${zypp_solver_detail_HEADERS}
  ${zypp_url_HEADERS}
  ${zypp_HEADERS}
  ${zypp_zypp_detail_HEADERS}
  ${zypp_repo_HEADERS}
  ${zypp_source_susetags_HEADERS}
  ${zypp_target_modalias_HEADERS}
  ${zypp_target_HEADERS}
  ${zypp_target_detail_HEADERS}
  ${zypp_pool_HEADERS}
  ${zypp_misc_HEADERS}
  ${zypp_core_compat_HEADERS}
  ${zypp_core_base_compat_HEADERS}
  ${zypp_core_url_compat_HEADERS}
  ${zypp_core_parser_compat_HEADERS}
  ${zypp_media_compat_HEADERS}

  ##########################

  ${zypp_media_HEADERS}
  ${zypp_media_detail_HEADERS}
  ng/context.h
  ng/media/provide.h
)

SET( zypp_SOURCES

  ##########################
  ${zypp_misc_SRCS}
  ${zypp_pool_SRCS}
  ${zypp_parser_susetags_SRCS}
  ${zypp_parser_xml_SRCS}
  ${zypp_parser_yum_SRCS}
  ${zypp_parser_SRCS}
  ${zypp_media_proxyinfo_SRCS}
  ${zypp_ng_SRCS}
  ${zypp_url_SRCS}
  ${zypp_repo_SRCS}
  ${zypp_repo_yum_SRCS}
  ${zypp_repo_susetags_SRCS}
  ${zypp_repo_data_SRCS}
  ${zypp_target_rpm_SRCS}
  ${zypp_target_hal_SRCS}
  ${zypp_target_modalias_SRCS}
  ${zypp_target_SRCS}
  ${zypp_sat_SRCS}
  ${zypp_sat_detail_SRCS}
  ${zypp_solver_detail_SRCS}
  ${zypp_ui_SRCS}
  ${zypp_SRCS}
  ${zypp_zypp_detail_SRCS}
  ${zypp_EARLY_SRCS}
  ${zypp_base_SRCS}
##########################

  ${zypp_media_SRCS}
  ng/context.cc
  ng/media/provide.cc
)

zypp_add_zypp_target( TARGETNAME zypp-objlib HEADERS ${zypp_HEADER} SOURCES ${zypp_SOURCES} FLAGS zypp_lib_compiler_flags INSTALL_HEADERS )

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
