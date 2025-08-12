include(${CMAKE_CURRENT_LIST_DIR}/../logic-helper.cmake)

function(zypp_add_zypp_target)

  FIND_PACKAGE(Rpm REQUIRED)
  IF ( NOT RPM_FOUND)
    MESSAGE( FATAL_ERROR " rpm-devel not found" )
  ENDIF ( NOT RPM_FOUND)

  set(oneValueArgs TARGETNAME)
  set(multiValueArgs HEADERS SOURCES )

  cmake_parse_arguments(PARSE_ARGV 0 arg "" "${oneValueArgs}" "${multiValueArgs}")

  zypp_add_sources( zypp_SRCS
    Application.cc
    Capabilities.cc
    Capability.cc
    CapMatch.cc
    Changelog.cc
    CpeId.cc
    Dep.cc
    Digest.cc
    DiskUsageCounter.cc
    DownloadMode.cc
    Edition.cc
    PluginScript.cc
    PluginScriptException.cc
    PluginExecutor.cc
    Fetcher.cc
    FileChecker.cc
    Glob.cc
    HistoryLog.cc
    HistoryLogData.cc
    IdString.cc
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
    Range.cc
    Rel.cc
    RepoInfo.cc
    RepoManager.cc
    RepoManagerOptions.cc
    Repository.cc
    RepoStatus.cc
    ResKind.cc
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
  zypp_add_sources( zypp_EARLY_SRCS
    ZConfig.cc
    Arch.cc
    Locale.cc
    CountryCode.cc
    LanguageCode.cc
  )

  zypp_add_sources( zypp_HEADERS
    Application.h
    Arch.h
    Bit.h
    Bitmap.h
    Callback.h
    Capabilities.h
    Capability.h
    CapMatch.h
    Changelog.h
    CheckSum.h
    CountryCode.h
    CpeId.h
    Dep.h
    Digest.h
    DiskUsageCounter.h
    DownloadMode.h
    Edition.h
    PluginScript.h
    PluginScriptException.h
    PluginExecutor.h
    Fetcher.h
    FileChecker.h
    Glob.h
    HistoryLog.h
    HistoryLogData.h
    IdString.h
    IdStringType.h
    InstanceId.h
    KeyContext.h
    KeyRing.h
    KeyRingContexts.h
    LanguageCode.h
    Locale.h
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
    Range.h
    RelCompare.h
    Rel.h
    RepoInfo.h
    RepoManager.h
    RepoManagerFlags.h
    RepoManagerOptions.h
    Repository.h
    RepoStatus.h
    Filter.h
    ResFilters.h
    ResKind.h
    ResObject.h
    ResObjects.h
    Resolvable.h
    Resolver.h
    ResolverFocus.h
    ResolverNamespace.h
    ResolverProblem.h
    ResPool.h
    ResPoolProxy.h
    ResStatus.h
    ResTraits.h
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

  zypp_add_sources( zypp_base_SRCS
    base/Backtrace.cc
    base/SerialNumber.cc
    base/Random.cc
    base/Measure.cc
    base/SetRelationMixin.cc
    base/StrMatcher.h
    base/StrMatcher.cc
  )

  zypp_add_sources( zypp_base_HEADERS
    base/Backtrace.h
    base/Collector.h
    base/SerialNumber.h
    base/Random.h
    base/Algorithm.h
    base/Counter.h
    base/Debug.h
    base/Functional.h
    base/Json.h
    base/LocaleGuard.h
    base/Measure.h
    base/NamedValue.h
    base/ReferenceCounted.h
    base/SetRelationMixin.h
    base/SetTracker.h
    base/Signal.h
    base/StrMatcher.h
    base/ValueTransform.h
  )

  INSTALL(  FILES
    ${zypp_base_HEADERS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/base
  )

  zypp_add_sources( zypp_ng_SRCS
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

  zypp_add_sources( zypp_ng_HEADERS
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
    ng/workflows/logichelpers.h
    ng/workflows/repoinfowf.h
    ng/workflows/signaturecheckwf.h
  )

  zypp_add_sources( zypp_ng_private_HEADERS
    ng/private/repomanager_p.h
  )

  #INSTALL( FILES
  #  ${zypp_ng_HEADERS}
  #  DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/repo
  #)


  zypp_add_sources( zypp_parser_SRCS
    parser/HistoryLogReader.cc
    parser/RepoFileReader.cc
    parser/RepoindexFileReader.cc
    parser/ServiceFileReader.cc
    parser/ProductFileReader.cc
  )

  zypp_add_sources( zypp_parser_HEADERS
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

  zypp_add_sources( zypp_parser_susetags_SRCS
    parser/susetags/RepoIndex.cc
    parser/susetags/ContentFileReader.cc
  )

  zypp_add_sources( zypp_parser_susetags_HEADERS
    parser/susetags/RepoIndex.h
    parser/susetags/ContentFileReader.h
  )

  INSTALL(  FILES
    ${zypp_parser_susetags_HEADERS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/parser/susetags
  )

  zypp_add_sources( zypp_parser_xml_SRCS
    parser/xml/Node.cc
    parser/xml/ParseDef.cc
    parser/xml/ParseDefConsume.cc
    parser/xml/ParseDefException.cc
    parser/xml/Reader.cc
    parser/xml/XmlString.cc
    parser/xml/libxmlfwd.cc
  )

  zypp_add_sources( zypp_parser_xml_HEADERS
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

  zypp_add_sources( zypp_parser_yum_SRCS
    parser/yum/RepomdFileReader.cc
  )

  zypp_add_sources( zypp_parser_yum_HEADERS
    parser/yum/RepomdFileReader.h
  )

  INSTALL(  FILES
    ${zypp_parser_yum_HEADERS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/parser/yum
  )

  zypp_add_sources( zypp_pool_SRCS
    pool/PoolImpl.cc
    pool/PoolStats.cc
  )

  zypp_add_sources( zypp_pool_HEADERS
    pool/PoolImpl.h
    pool/PoolStats.h
    pool/PoolTraits.h
    pool/ByIdent.h
  )

  INSTALL(  FILES
    ${zypp_pool_HEADERS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/pool
  )

  # zypp_add_sources( zypp_solver_detail_SRCS )

  zypp_add_sources( zypp_solver_HEADERS
    solver/Types.h
  )

  INSTALL(  FILES
    ${zypp_solver_HEADERS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/solver
  )

  zypp_add_sources( zypp_solver_detail_SRCS
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

  zypp_add_sources( zypp_solver_detail_HEADERS
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

  zypp_add_sources( zypp_sat_SRCS
    sat/Pool.cc
    sat/Solvable.cc
    sat/SolvableSet.cc
    sat/SolvableSpec.cc
    sat/SolvIterMixin.cc
    sat/Map.cc
    sat/Queue.cc
    sat/FileConflicts.cc
    sat/Transaction.cc
    sat/WhatProvides.cc
    sat/WhatObsoletes.cc
    sat/LocaleSupport.cc
    sat/LookupAttr.cc
    sat/SolvAttr.cc
  )

  zypp_add_sources( zypp_sat_HEADERS
    sat/Pool.h
    sat/Solvable.h
    sat/SolvableSet.h
    sat/SolvableType.h
    sat/SolvableSpec.h
    sat/SolvIterMixin.h
    sat/Map.h
    sat/Queue.h
    sat/FileConflicts.h
    sat/Transaction.h
    sat/WhatProvides.h
    sat/WhatObsoletes.h
    sat/LocaleSupport.h
    sat/LookupAttr.h
    sat/LookupAttrTools.h
    sat/SolvAttr.h
  )

  INSTALL(  FILES
    ${zypp_sat_HEADERS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/sat
  )

  zypp_add_sources( zypp_sat_detail_SRCS
    sat/detail/PoolImpl.cc
  )

  zypp_add_sources( zypp_sat_detail_HEADERS
    sat/detail/PoolMember.h
    sat/detail/PoolImpl.h
  )

  INSTALL(  FILES
    ${zypp_sat_detail_HEADERS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/sat/detail
  )


  zypp_add_sources( zypp_target_SRCS
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

  zypp_add_sources( zypp_target_HEADERS
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


  zypp_add_sources( zypp_target_detail_HEADERS
    target/private/commitpackagepreloader_p.h
  )

  INSTALL(  FILES
    ${zypp_target_HEADERS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/target
  )

  zypp_add_sources( zypp_target_modalias_SRCS
    target/modalias/Modalias.cc
  )

  zypp_add_sources( zypp_target_modalias_HEADERS
    target/modalias/Modalias.h
  )

  INSTALL(  FILES
    ${zypp_target_modalias_HEADERS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/target/modalias
  )

  zypp_add_sources( zypp_target_rpm_SRCS
    target/rpm/BinHeader.cc
    target/rpm/RpmCallbacks.cc
    target/rpm/RpmDb.cc
    target/rpm/RpmException.cc
    target/rpm/RpmHeader.cc
    target/rpm/librpmDb.cc
  )

  zypp_add_sources( zypp_target_rpm_HEADERS
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

  zypp_add_sources( zypp_ui_SRCS
    ui/Selectable.cc
    ui/SelectableImpl.cc
    ui/Status.cc
    ui/UserWantedPackages.cc
  )

  zypp_add_sources( zypp_ui_HEADERS
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

  zypp_add_sources( zypp_zypp_detail_SRCS
    zypp_detail/ZYppImpl.cc
  )

  zypp_add_sources( zypp_zypp_detail_HEADERS
    zypp_detail/keyring_p.h
    zypp_detail/urlcredentialextractor_p.h
    zypp_detail/ZYppImpl.h
    zypp_detail/ZYppReadOnlyHack.h
  )

  INSTALL(  FILES
    ${zypp_zypp_detail_HEADERS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/zypp_detail
  )

  zypp_add_sources( zypp_repo_SRCS
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
    repo/MediaInfoDownloader.cc
    repo/RepoVariables.cc
    repo/RepoInfoBase.cc
    repo/PluginRepoverification.cc
    repo/PluginServices.cc
  )

  zypp_add_sources( zypp_repo_HEADERS
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
    repo/MediaInfoDownloader.h
    repo/RepoVariables.h
    repo/RepoInfoBase.h
    repo/PluginRepoverification.h
    repo/PluginServices.h
  )

  INSTALL( FILES
    ${zypp_repo_HEADERS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/zypp/repo
  )

  zypp_add_sources( zypp_repo_yum_SRCS
    repo/yum/RepomdFileCollector.cc
  )

  zypp_add_sources( zypp_repo_yum_HEADERS
    repo/yum/RepomdFileCollector.h
  )

  zypp_add_sources( zypp_repo_susetags_SRCS
  )

  zypp_add_sources( zypp_repo_susetags_HEADERS
  )


  ####################################################################

  zypp_add_sources( zypp_misc_HEADERS
    Misc.h
    misc/DefaultLoadSystem.h
    misc/CheckAccessDeleted.h
    misc/TestcaseSetup.h
    misc/LoadTestcase.h
  )

  zypp_add_sources( zypp_misc_SRCS
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

  SET( zypp_lib_SRCS
    ${arg_SOURCES}
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
    ${zypp_solver_detail_SRCS}
    ${zypp_ui_SRCS}
    ${zypp_SRCS}
    ${zypp_zypp_detail_SRCS}
    ${zypp_sat_SRCS}
    ${zypp_sat_detail_SRCS}
    ${zypp_EARLY_SRCS}
    ${zypp_base_SRCS}
  )

  SET( zypp_lib_HEADERS
    ${arg_HEADERS}
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
    ${zypp_solver_HEADERS}
    ${zypp_solver_detail_HEADERS}
    ${zypp_sat_HEADERS}
    ${zypp_sat_detail_HEADERS}
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
  )

  # Default loggroup for all files
  SET_LOGGROUP( "zypp" ${zypp_lib_SRCS} )

  # override some defaults
  SET_LOGGROUP( "libsolv" ${zypp_sat_SRCS} )

  if ( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

    message(NOTICE "Enabling static analysis: ${ZYPP_CXX_CLANG_TIDY} ${ZYPP_CXX_CPPCHECK}")

    set_source_files_properties(
        target/TargetImpl.cc
        PROPERTIES
        SKIP_LINTING ON
    )

    if (ZYPP_CXX_CLANG_TIDY)
      set( CMAKE_CXX_CLANG_TIDY ${ZYPP_CXX_CLANG_TIDY} )
    endif(ZYPP_CXX_CLANG_TIDY)

    if (ZYPP_CXX_CPPCHECK)
      set(CMAKE_CXX_CPPCHECK ${ZYPP_CXX_CPPCHECK})
    endif(ZYPP_CXX_CPPCHECK)

  endif( ZYPP_CXX_CLANG_TIDY OR ZYPP_CXX_CPPCHECK )

  ADD_LIBRARY( ${arg_TARGETNAME} OBJECT ${zypp_lib_SRCS} ${zypp_lib_HEADERS}  )

  get_filename_component(PARENT_LIST_DIR ${CMAKE_CURRENT_FUNCTION_LIST_DIR}   DIRECTORY)
  get_filename_component(PARENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}   DIRECTORY)
  get_filename_component(PARENT_BIN_DIR  ${CMAKE_CURRENT_BINARY_DIR} DIRECTORY)

  zypp_logic_setup_includes()

  target_link_libraries( ${arg_TARGETNAME} zypp_lib_compiler_flags )

  target_include_directories( ${arg_TARGETNAME} PUBLIC ${RPM_INCLUDE_DIR})

  IF (ENABLE_ZCHUNK_COMPRESSION)
    target_include_directories( ${arg_TARGETNAME} PRIVATE ${ZCHUNK_INCLUDEDIR} )
  ENDIF(ENABLE_ZCHUNK_COMPRESSION)

  # fix includes not relative to rpm
  target_include_directories( ${arg_TARGETNAME} PUBLIC ${RPM_INCLUDE_DIR}/rpm)

  # rpm verify function and callback states were introduced in rpm-4.15
  if( RPM_LIB_VER VERSION_GREATER_EQUAL "4.15.0"  AND  RPM_LIB_VER VERSION_LESS "5.0.0")
    target_compile_definitions( ${arg_TARGETNAME} PUBLIC HAVE_RPM_VERIFY_TRANSACTION_STEP )
  endif()

  if( RPM_LIB_VER VERSION_GREATER_EQUAL "5.0.0" )
        MESSAGE( STATUS "rpm found: enable rpm-4 compat interface." )
        target_compile_definitions( ${arg_TARGETNAME} PUBLIC _RPM_5)
  endif ()


endfunction()

