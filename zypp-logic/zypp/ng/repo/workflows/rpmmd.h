/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_RPMMD_WORKFLOW_INCLUDED
#define ZYPP_NG_RPMMD_WORKFLOW_INCLUDED


#include <zypp/RepoInfo.h>
#include <zypp/ng/repo/Downloader>
#include <zypp-core/ng/pipelines/AsyncResult>
#include <zypp-core/ng/pipelines/Expected>

#include <zypp/ManagedFile.h>


namespace zyppng {
  class ProvideMediaHandle;

  ZYPP_FWD_DECL_TYPE_WITH_REFS( ProgressObserver );

   /**
    * \short Downloader workspace for YUM (rpm-nmd) repositories
    * Encapsulates all the knowledge of which files have
    * to be downloaded to the local disk.
    */
  namespace RpmmdWorkflows {
    MaybeAwaitable<expected<zypp::RepoStatus>> repoStatus( repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle );

    MaybeAwaitable<expected<repo::DownloadContextRef>> download ( repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver = nullptr );
  }
}


#endif
