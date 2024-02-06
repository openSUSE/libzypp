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

#include <zypp/zypp_detail/repomanagerbase_p.h>

// for the status enums, we should refactor those out
#include <zypp/RepoManager.h>

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

    AsyncOpRef<expected<repo::RefreshCheckStatus> > checkIfToRefreshMetadata( repo::AsyncRefreshContextRef refCtx, ProvideMediaHandle medium, ProgressObserverRef progressObserver = nullptr );
    expected<repo::RefreshCheckStatus> checkIfToRefreshMetadata( repo::SyncRefreshContextRef refCtx, SyncMediaHandle medium, ProgressObserverRef progressObserver = nullptr );

    AsyncOpRef<expected<repo::AsyncRefreshContextRef> > refreshMetadata( repo::AsyncRefreshContextRef refCtx, ProvideMediaHandle medium, ProgressObserverRef progressObserver = nullptr );
    expected<repo::SyncRefreshContextRef> refreshMetadata( repo::SyncRefreshContextRef refCtx, SyncMediaHandle medium, ProgressObserverRef progressObserver = nullptr );

    AsyncOpRef<expected<repo::AsyncRefreshContextRef> > buildCache( repo::AsyncRefreshContextRef refCtx, ProgressObserverRef progressObserver = nullptr );
    expected<repo::SyncRefreshContextRef> buildCache( repo::SyncRefreshContextRef refCtx, ProgressObserverRef progressObserver = nullptr );

  }
}


#endif
