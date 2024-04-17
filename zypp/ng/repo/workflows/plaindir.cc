/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "plaindir.h"

#include <zypp-core/zyppng/ui/ProgressObserver>
#include <zypp-media/ng/ProvideSpec>
#include <zypp/ng/Context>

#include <zypp/ng/workflows/logichelpers.h>
#include <zypp/ng/workflows/contextfacade.h>
#include <zypp/ng/repo/workflows/repodownloaderwf.h>
#include <zypp/ng/workflows/checksumwf.h>

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::repomanager"

namespace zyppng::PlaindirWorkflows {

  namespace {
    template<typename DlContextRefType, typename MediaHandle>
    auto statusLogic( DlContextRefType &&ctx, MediaHandle mediaHandle ) {
      constexpr bool isAsync = std::is_same_v<DlContextRefType,repo::AsyncDownloadContextRef>;

      // this can only happen if this function is called with a non mounting medium, but those do not support plaindir anyway
      if ( !mediaHandle.localPath().has_value() ) {
        return makeReadyResult<expected<zypp::RepoStatus>, isAsync>( expected<zypp::RepoStatus>::error( ZYPP_EXCPT_PTR( zypp::Exception("Medium does not support plaindir") )) );
      }

      // dir status
      const auto &repoInfo = std::forward<DlContextRefType>(ctx)->repoInfo();
      auto rStatus = zypp::RepoStatus( repoInfo ) && zypp::RepoStatus( mediaHandle.localPath().value() / repoInfo.path() );
      return makeReadyResult<expected<zypp::RepoStatus>, isAsync> ( expected<zypp::RepoStatus>::success(std::move(rStatus)) );
    }
  }

  AsyncOpRef<expected<zypp::RepoStatus> > repoStatus(repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle)
  {
    return statusLogic( std::move(dl), std::move(mediaHandle) );
  }

  expected<zypp::RepoStatus> repoStatus(repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle)
  {
    return statusLogic( std::move(dl), std::move(mediaHandle) );
  }


  namespace {
    template<typename DlContextRefType, typename MediaHandle>
    auto dlLogic( DlContextRefType &&ctx, MediaHandle mediaHandle, ProgressObserverRef progressObserver ) {

      constexpr bool isAsync = std::is_same_v<DlContextRefType,repo::AsyncDownloadContextRef>;
      using Ret = expected<DlContextRefType>;

      try {
        // this can only happen if this function is called with a non mounting medium, but those do not support plaindir anyway
        if ( !mediaHandle.localPath().has_value() ) {
          return makeReadyResult<Ret, isAsync>( Ret::error( ZYPP_EXCPT_PTR( zypp::Exception("Medium does not support plaindir") )) );
        }

        if ( progressObserver ) progressObserver->inc();

        // as substitute for real metadata remember the checksum of the directory we refreshed
        const auto &repoInfo = std::forward<DlContextRefType>(ctx)->repoInfo();
        auto newstatus = zypp::RepoStatus( mediaHandle.localPath().value() / repoInfo.path() );	// dir status

        zypp::Pathname productpath( std::forward<DlContextRefType>(ctx)->destDir() / repoInfo.path() );
        zypp::filesystem::assert_dir( productpath );
        newstatus.saveToCookieFile( productpath/"cookie" );

        if ( progressObserver ) progressObserver->setFinished();

      } catch ( const zypp::Exception &e ) {
        ZYPP_CAUGHT(e);
        return makeReadyResult<Ret, isAsync>( Ret::error( ZYPP_FWD_CURRENT_EXCPT() ) );
      } catch ( ... ) {
        return makeReadyResult<Ret, isAsync>( Ret::error( ZYPP_FWD_CURRENT_EXCPT() ) );
      }
      return makeReadyResult<Ret, isAsync>( Ret::success( std::forward<DlContextRefType>(ctx) ) );
    }
  }

  AsyncOpRef<expected<repo::AsyncDownloadContextRef> > download(repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver)
  {
    return dlLogic( std::move(dl), std::move(mediaHandle), std::move(progressObserver) );
  }

  expected<repo::SyncDownloadContextRef> download(repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle, ProgressObserverRef progressObserver)
  {
    return dlLogic( std::move(dl), std::move(mediaHandle), std::move(progressObserver) );
  }

}
