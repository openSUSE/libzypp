/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_REPOMANAGER_WORKFLOW_INCLUDED
#define ZYPP_NG_REPOMANAGER_WORKFLOW_INCLUDED


#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp-core/zyppng/pipelines/Expected>

#include <zypp/ng/repomanager.h>
#include <zypp/RepoManagerFlags.h>

//@ TODO move required types into their own files... e.g. CheckStatus
#include <zypp/ng/repo/Refresh>


namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS (Context);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (SyncContext);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (ProgressObserver);

  class ProvideMediaHandle;
  class SyncMediaHandle;

  namespace RepoManagerWorkflow {

    AsyncOpRef<expected<zypp::repo::RepoType> > probeRepoType( ContextRef ctx, ProvideMediaHandle medium, zypp::Pathname path, std::optional<zypp::Pathname> targetPath = {} );
    expected<zypp::repo::RepoType> probeRepoType ( SyncContextRef ctx, SyncMediaHandle medium, zypp::Pathname path, std::optional<zypp::Pathname> targetPath = {} );

    AsyncOpRef<expected<zypp::repo::RepoType> > probeRepoType( ContextRef ctx, RepoInfo repo, std::optional<zypp::Pathname> targetPath = {} );
    expected<zypp::repo::RepoType> probeRepoType ( SyncContextRef ctx, RepoInfo repo, std::optional<zypp::Pathname> targetPath = {} );

    AsyncOpRef<expected<std::list<RepoInfo>>> readRepoFile( ContextRef ctx, zypp::Url repoFileUrl );
    expected<std::list<RepoInfo>> readRepoFile( SyncContextRef ctx, zypp::Url repoFileUrl );

    AsyncOpRef<expected<repo::RefreshCheckStatus> > checkIfToRefreshMetadata( repo::AsyncRefreshContextRef refCtx, ProvideMediaHandle medium, ProgressObserverRef progressObserver = nullptr );
    expected<repo::RefreshCheckStatus> checkIfToRefreshMetadata( repo::SyncRefreshContextRef refCtx, SyncMediaHandle medium, ProgressObserverRef progressObserver = nullptr );

    AsyncOpRef<expected<repo::AsyncRefreshContextRef> > refreshMetadata( repo::AsyncRefreshContextRef refCtx, ProgressObserverRef progressObserver = nullptr );
    expected<repo::SyncRefreshContextRef> refreshMetadata( repo::SyncRefreshContextRef refCtx, ProgressObserverRef progressObserver = nullptr );

    AsyncOpRef<expected<repo::AsyncRefreshContextRef> > refreshMetadata( repo::AsyncRefreshContextRef refCtx, ProvideMediaHandle medium, ProgressObserverRef progressObserver = nullptr );
    expected<repo::SyncRefreshContextRef> refreshMetadata( repo::SyncRefreshContextRef refCtx, SyncMediaHandle medium, ProgressObserverRef progressObserver = nullptr );

    AsyncOpRef<expected<repo::AsyncRefreshContextRef> > buildCache( repo::AsyncRefreshContextRef refCtx, zypp::RepoManagerFlags::CacheBuildPolicy policy, ProgressObserverRef progressObserver = nullptr );
    expected<repo::SyncRefreshContextRef> buildCache( repo::SyncRefreshContextRef refCtx, zypp::RepoManagerFlags::CacheBuildPolicy policy, ProgressObserverRef progressObserver = nullptr );

    AsyncOpRef<expected<RepoInfo>> addRepository( AsyncRepoManagerRef mgr, RepoInfo info, ProgressObserverRef myProgress = nullptr );
    expected<RepoInfo> addRepository( SyncRepoManagerRef mgr, const RepoInfo &info, ProgressObserverRef myProgress = nullptr );

    AsyncOpRef<expected<void>> addRepositories( AsyncRepoManagerRef mgr, zypp::Url url, ProgressObserverRef myProgress = nullptr );
    expected<void> addRepositories( SyncRepoManagerRef mgr, zypp::Url url, ProgressObserverRef myProgress = nullptr );

    AsyncOpRef<expected<void>> refreshGeoIPData( ContextRef ctx, RepoInfo::url_set urls );
    expected<void> refreshGeoIPData( SyncContextRef ctx, RepoInfo::url_set urls );

  }
}


#endif
