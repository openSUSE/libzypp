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


#include <zypp-core/ng/async/task.h>
#include <zypp-core/ng/pipelines/Expected>
#include <zypp-media/ng/LazyMediaHandle>

#include <zypp/ng/repomanager.h>
#include <zypp/RepoManagerFlags.h>

//@ TODO move required types into their own files... e.g. CheckStatus
#include <zypp/ng/repo/Refresh>

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS (Context);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (ProgressObserver);

  class Provide;

  namespace RepoManagerWorkflow {

    MaybeAwaitable<expected<zypp::repo::RepoType> > probeRepoType( ContextRef ctx,  LazyMediaHandle<Provide> medium, zypp::Pathname path, std::optional<zypp::Pathname> targetPath = {} );

    MaybeAwaitable<expected<zypp::repo::RepoType> > probeRepoType( ContextRef ctx, RepoInfo repo, std::optional<zypp::Pathname> targetPath = {} );

    MaybeAwaitable<expected<std::list<RepoInfo>>> readRepoFile( ContextRef ctx, zypp::Url repoFileUrl );

    MaybeAwaitable<expected<repo::RefreshCheckStatus> > checkIfToRefreshMetadata( repo::RefreshContextRef refCtx,  LazyMediaHandle<Provide> medium, ProgressObserverRef progressObserver = nullptr );

    MaybeAwaitable<expected<repo::RefreshContextRef> > refreshMetadata( repo::RefreshContextRef refCtx, ProgressObserverRef progressObserver = nullptr );

    MaybeAwaitable<expected<repo::RefreshContextRef> > refreshMetadata( repo::RefreshContextRef refCtx,  LazyMediaHandle<Provide> medium, ProgressObserverRef progressObserver = nullptr );

    MaybeAwaitable<expected<repo::RefreshContextRef> > buildCache( repo::RefreshContextRef refCtx, zypp::RepoManagerFlags::CacheBuildPolicy policy, ProgressObserverRef progressObserver = nullptr );

    MaybeAwaitable<expected<RepoInfo>> addRepository( RepoManagerRef mgr, RepoInfo info, ProgressObserverRef myProgress = nullptr, const zypp::TriBool & forcedProbe = zypp::indeterminate );

    MaybeAwaitable<expected<void>> addRepositories( RepoManagerRef mgr, zypp::Url url, ProgressObserverRef myProgress = nullptr );

    MaybeAwaitable<expected<void>> refreshGeoIPData( ContextRef ctx, RepoInfo::url_set urls );

    MaybeAwaitable<expected<void>> refreshGeoIPData( ContextRef ctx, zypp::MirroredOriginSet origins );

  }
}


#endif
