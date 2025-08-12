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
    MaybeAwaitable<expected<zypp::RepoStatus>> repoStatus( repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle );

    MaybeAwaitable<expected<repo::DownloadContextRef>> download ( repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver = nullptr );
  }
}



#endif // ZYPP_NG_PLAINDIR_WORKFLOW_INCLUDED
