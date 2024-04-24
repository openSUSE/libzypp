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
#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp-core/zyppng/pipelines/Expected>

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
    AsyncOpRef<expected<zypp::RepoStatus>> repoStatus( repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle );
    expected<zypp::RepoStatus> repoStatus( repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle );

    /*!
     * Download metadata to a local directory
     */
    AsyncOpRef<expected<repo::AsyncDownloadContextRef>> download ( repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver = nullptr ) ZYPP_TESTS;
    expected<repo::SyncDownloadContextRef> download ( repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle, ProgressObserverRef progressObserver = nullptr ) ZYPP_TESTS;
  }
}



#endif // ZYPP_NG_SUSETAGS_WORKFLOW_INCLUDED
