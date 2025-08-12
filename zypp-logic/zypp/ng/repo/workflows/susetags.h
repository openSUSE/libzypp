/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_SUSETAGS_WORKFLOW_INCLUDED
#define ZYPP_NG_SUSETAGS_WORKFLOW_INCLUDED

#include <zypp/RepoInfo.h>
#include <zypp/ng/repo/Downloader>
#include <zypp-core/ng/pipelines/AsyncResult>
#include <zypp-core/ng/pipelines/Expected>

#include <zypp/ManagedFile.h>


namespace zyppng {
  class ProvideMediaHandle;
  class SyncMediaHandle;

  ZYPP_FWD_DECL_TYPE_WITH_REFS( ProgressObserver );

 /**
  * \short Download workflow namespace for SUSETags (YaST2) repositories
  * Encapsulates all the knowledge of which files have
  * to be downloaded to the local disk and how to calculate the repo status.
  */
  namespace SuseTagsWorkflows {

    /*!
     * Calculate status of the remote SUSETags repository
     */
    MaybeAwaitable<expected<zypp::RepoStatus>> repoStatus( repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle );

    /*!
     * Download metadata to a local directory
     */
    MaybeAwaitable<expected<repo::DownloadContextRef>> download ( repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver = nullptr );
  }
}



#endif // ZYPP_NG_SUSETAGS_WORKFLOW_INCLUDED
