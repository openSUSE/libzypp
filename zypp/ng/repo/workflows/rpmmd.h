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
#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp-core/zyppng/pipelines/Expected>

#include <zypp/ManagedFile.h>


namespace zyppng {
  class ProvideMediaHandle;
  class SyncMediaHandle;

  ZYPP_FWD_DECL_TYPE_WITH_REFS( ProgressObserver );

   /**
    * \short Downloader workspace for YUM (rpm-nmd) repositories
    * Encapsulates all the knowledge of which files have
    * to be downloaded to the local disk.
    */
  namespace RpmmdWorkflows {
    AsyncOpRef<expected<zypp::RepoStatus>> repoStatus(repo::AsyncDownloadContextRef dl, ProgressObserverRef progressObserver , ProvideMediaHandle mediaHandle);
    expected<zypp::RepoStatus> repoStatus(repo::SyncDownloadContextRef dl, ProgressObserverRef progressObserver,  SyncMediaHandle mediaHandle );

    AsyncOpRef<expected<repo::AsyncDownloadContextRef>> download ( repo::AsyncDownloadContextRef dl, ProgressObserverRef progressObserver, ProvideMediaHandle mediaHandle );
    expected<repo::SyncDownloadContextRef> download ( repo::SyncDownloadContextRef dl, ProgressObserverRef progressObserver, SyncMediaHandle mediaHandle );
  }
}


#endif
