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
#include <zypp-core/ng/ui/ProgressObserver>
#include <zypp-core/ng/pipelines/AsyncResult>
#include <zypp-core/ng/pipelines/Expected>
#include <zypp-media/MediaException>
#include <zypp-media/ng/ProvideSpec>
#include <zypp-media/ng/LazyMediaHandle>

#include <zypp/ManagedFile.h>


namespace zyppng {

  class Provide;
  class ProvideMediaHandle;

  namespace RepoDownloaderWorkflow {
    MaybeAwaitable<expected<repo::DownloadContextRef>> downloadMasterIndex ( repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle, zypp::filesystem::Pathname masterIndex_r );
    MaybeAwaitable<expected<repo::DownloadContextRef>> downloadMasterIndex ( repo::DownloadContextRef dl, LazyMediaHandle<Provide> mediaHandle, zypp::filesystem::Pathname masterIndex_r );

    MaybeAwaitable<expected<zypp::RepoStatus>>  repoStatus( repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle );
    MaybeAwaitable<expected<zypp::RepoStatus> > repoStatus( repo::DownloadContextRef dl, LazyMediaHandle<Provide> mediaHandle );

    MaybeAwaitable<expected<repo::DownloadContextRef>> download ( repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver = nullptr );
    MaybeAwaitable<expected<repo::DownloadContextRef> > download(repo::DownloadContextRef dl, LazyMediaHandle<Provide> mediaHandle, ProgressObserverRef progressObserver);

    template <typename MediaHandle>
    auto downloadMediaInfo ( MediaHandle &&mediaHandle, const zypp::filesystem::Pathname &destdir ) {
      using namespace zyppng::operators;
      using ProvideType = typename std::decay_t<MediaHandle>::ParentType;

      auto provider = mediaHandle.parent();
      if ( !provider )
        return makeReadyResult<expected<zypp::ManagedFile>>( expected<zypp::ManagedFile>::error(ZYPP_EXCPT_PTR(zypp::media::MediaException("Invalid handle"))) );

      return provider->provide( std::forward<MediaHandle>(mediaHandle), "/media.1/media", ProvideFileSpec().setOptional(true).setDownloadSize( zypp::ByteCount(20, zypp::ByteCount::MB ) ).setMirrorsAllowed(false) )
             | and_then( ProvideType::copyResultToDest( provider, destdir / "/media.1/media" ) );

    }
  }
}


#endif
