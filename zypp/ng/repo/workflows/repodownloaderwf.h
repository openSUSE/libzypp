/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_REPODOWNLOADER_WORKFLOW_INCLUDED
#define ZYPP_NG_REPODOWNLOADER_WORKFLOW_INCLUDED


#include <zypp/RepoInfo.h>
#include <zypp/ng/repo/Downloader>
#include <zypp-core/zyppng/ui/ProgressObserver>
#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp-media/MediaException>
#include <zypp-media/ng/ProvideSpec>
#include <zypp-media/ng/LazyMediaHandle>

#include <zypp/ManagedFile.h>


namespace zyppng {

  class Provide;
  class MediaSyncFacade;

  class ProvideMediaHandle;
  class SyncMediaHandle;

  using AsyncLazyMediaHandle = LazyMediaHandle<Provide>;
  using SyncLazyMediaHandle  = LazyMediaHandle<MediaSyncFacade>;

  namespace RepoDownloaderWorkflow {
    AsyncOpRef<expected<repo::AsyncDownloadContextRef>> downloadMasterIndex ( repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle, zypp::filesystem::Pathname masterIndex_r );
    AsyncOpRef<expected<repo::AsyncDownloadContextRef>> downloadMasterIndex ( repo::AsyncDownloadContextRef dl, AsyncLazyMediaHandle mediaHandle, zypp::filesystem::Pathname masterIndex_r );
    expected<repo::SyncDownloadContextRef> downloadMasterIndex ( repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle, zypp::filesystem::Pathname masterIndex_r );
    expected<repo::SyncDownloadContextRef> downloadMasterIndex ( repo::SyncDownloadContextRef dl, SyncLazyMediaHandle mediaHandle, zypp::filesystem::Pathname masterIndex_r );

    AsyncOpRef<expected<zypp::RepoStatus>>  repoStatus( repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle );
    AsyncOpRef<expected<zypp::RepoStatus> > repoStatus( repo::AsyncDownloadContextRef dl, AsyncLazyMediaHandle mediaHandle );
    expected<zypp::RepoStatus> repoStatus( repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle );
    expected<zypp::RepoStatus> repoStatus( repo::SyncDownloadContextRef dl, SyncLazyMediaHandle mediaHandle );

    AsyncOpRef<expected<repo::AsyncDownloadContextRef>> download ( repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver = nullptr );
    AsyncOpRef<expected<repo::AsyncDownloadContextRef> > download(repo::AsyncDownloadContextRef dl, AsyncLazyMediaHandle mediaHandle, ProgressObserverRef progressObserver);
    expected<repo::SyncDownloadContextRef> download ( repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle, ProgressObserverRef progressObserver = nullptr );
    expected<repo::SyncDownloadContextRef> download(repo::SyncDownloadContextRef dl, SyncLazyMediaHandle mediaHandle, ProgressObserverRef progressObserver);

    template <typename MediaHandle>
    auto downloadMediaInfo ( MediaHandle &&mediaHandle, const zypp::filesystem::Pathname &destdir ) {
      using namespace zyppng::operators;
      using ProvideType = typename std::decay_t<MediaHandle>::ParentType;

      // check if we have a async media handle
      constexpr bool isAsync = std::is_same_v<ProvideType, Provide>;
      auto provider = mediaHandle.parent();
      if ( !provider )
        return makeReadyResult<expected<zypp::ManagedFile>, isAsync>( expected<zypp::ManagedFile>::error(ZYPP_EXCPT_PTR(zypp::media::MediaException("Invalid handle"))) );

      return provider->provide( std::forward<MediaHandle>(mediaHandle), "/media.1/media", ProvideFileSpec().setOptional(true).setDownloadSize( zypp::ByteCount(20, zypp::ByteCount::MB ) ))
             | and_then( ProvideType::copyResultToDest( provider, destdir / "/media.1/media" ) );

    }
  }
}


#endif
