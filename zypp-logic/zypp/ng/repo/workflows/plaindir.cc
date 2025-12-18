/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "plaindir.h"

#include <zypp-core/ng/ui/ProgressObserver>
#include <zypp-media/ng/ProvideSpec>
#include <zypp/ng/Context>


#include <zypp/ng/repo/workflows/repodownloaderwf.h>
#include <zypp/ng/workflows/checksumwf.h>

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::repomanager"

namespace zyppng::PlaindirWorkflows {

  namespace {
    auto statusLogic( repo::DownloadContextRef &&ctx, ProvideMediaHandle mediaHandle ) {
      // this can only happen if this function is called with a non mounting medium, but those do not support plaindir anyway
      if ( !mediaHandle.localPath().has_value() ) {
        return makeReadyTask<expected<zypp::RepoStatus>>( expected<zypp::RepoStatus>::error( ZYPP_EXCPT_PTR( zypp::Exception("Medium does not support plaindir") )) );
      }

      // dir status
      const auto &repoInfo = std::forward<repo::DownloadContextRef>(ctx)->repoInfo();
      auto rStatus = zypp::RepoStatus( repoInfo ) && zypp::RepoStatus( mediaHandle.localPath().value() / repoInfo.path() );
      return makeReadyTask<expected<zypp::RepoStatus>> ( expected<zypp::RepoStatus>::success(std::move(rStatus)) );
    }
  }

  MaybeAwaitable<expected<zypp::RepoStatus> > repoStatus(repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle)
  {
    return statusLogic( std::move(dl), std::move(mediaHandle) );
  }

  namespace {
    auto dlLogic( repo::DownloadContextRef &&ctx, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver ) {
      using Ret = expected<repo::DownloadContextRef>;

      try {
        // this can only happen if this function is called with a non mounting medium, but those do not support plaindir anyway
        if ( !mediaHandle.localPath().has_value() ) {
          return makeReadyTask<Ret>( Ret::error( ZYPP_EXCPT_PTR( zypp::Exception("Medium does not support plaindir") )) );
        }

        if ( progressObserver ) progressObserver->inc();

        // as substitute for real metadata remember the checksum of the directory we refreshed
        const auto &repoInfo = std::forward<repo::DownloadContextRef>(ctx)->repoInfo();
        auto newstatus = zypp::RepoStatus( mediaHandle.localPath().value() / repoInfo.path() );	// dir status

        zypp::Pathname productpath( std::forward<repo::DownloadContextRef>(ctx)->destDir() / repoInfo.path() );
        zypp::filesystem::assert_dir( productpath );
        newstatus.saveToCookieFile( productpath/"cookie" );

        if ( progressObserver ) progressObserver->setFinished();

      } catch ( const zypp::Exception &e ) {
        ZYPP_CAUGHT(e);
        return makeReadyTask<Ret>( Ret::error( ZYPP_FWD_CURRENT_EXCPT() ) );
      } catch ( ... ) {
        return makeReadyTask<Ret>( Ret::error( ZYPP_FWD_CURRENT_EXCPT() ) );
      }
      return makeReadyTask<Ret>( Ret::success( std::forward<repo::DownloadContextRef>(ctx) ) );
    }
  }

  MaybeAwaitable<expected<repo::DownloadContextRef> > download(repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver)
  {
    return dlLogic( std::move(dl), std::move(mediaHandle), std::move(progressObserver) );
  }
}
