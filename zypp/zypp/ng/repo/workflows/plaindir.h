/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_PLAINDIR_WORKFLOW_INCLUDED
#define ZYPP_NG_PLAINDIR_WORKFLOW_INCLUDED

#include <zypp/RepoInfo.h>
#include <zypp/ng/repo/Downloader>
#include <zypp-core/ng/pipelines/AsyncResult>
#include <zypp-core/ng/pipelines/Expected>

#include <zypp/ManagedFile.h>


namespace zyppng {
  class ProvideMediaHandle;
  class SyncMediaHandle;

  ZYPP_FWD_DECL_TYPE_WITH_REFS( ProgressObserver );

  namespace PlaindirWorkflows {
    AsyncOpRef<expected<zypp::RepoStatus>> repoStatus( repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle );
    expected<zypp::RepoStatus> repoStatus( repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle );

    AsyncOpRef<expected<repo::AsyncDownloadContextRef>> download ( repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver = nullptr );
    expected<repo::SyncDownloadContextRef> download ( repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle, ProgressObserverRef progressObserver = nullptr );
  }
}



#endif // ZYPP_NG_PLAINDIR_WORKFLOW_INCLUDED
