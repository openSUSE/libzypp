/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "rpmmd.h"
#include <zypp-core/zyppng/ui/ProgressObserver>
#include <zypp-media/ng/ProvideSpec>
#include <zypp/ng/Context>

#include <zypp/ng/workflows/logichelpers.h>
#include <zypp/ng/workflows/contextfacade.h>
#include <zypp/ng/repo/workflows/repodownloaderwf.h>
#include <zypp/parser/yum/RepomdFileReader.h>
#include <zypp/repo/yum/RepomdFileCollector.h>
#include <zypp/ng/workflows/checksumwf.h>

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::repomanager"

namespace zyppng::RpmmdWorkflows {

  namespace {

    using namespace zyppng::operators;

    template<class Executor, class OpType>
    struct StatusLogic : public LogicBase<Executor, OpType>{
      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

    public:

      using DlContextRefType = std::conditional_t<zyppng::detail::is_async_op_v<OpType>, repo::AsyncDownloadContextRef, repo::SyncDownloadContextRef>;
      using ZyppContextType = typename DlContextRefType::element_type::ContextType;
      using ProvideType     = typename ZyppContextType::ProvideType;
      using MediaHandle     = typename ProvideType::MediaHandle;
      using ProvideRes      = typename ProvideType::Res;

      StatusLogic( DlContextRefType ctx, MediaHandle &&media  )
        : _ctx(std::move(ctx))
        , _handle(std::move(media))
      {}

      MaybeAsyncRef<expected<zypp::RepoStatus>> execute() {
        return _ctx->zyppContext()->provider()->provide( _handle, _ctx->repoInfo().path() / "/repodata/repomd.xml" , ProvideFileSpec() )
          | [this]( expected<ProvideRes> repomdFile ) {

              if ( !repomdFile )
                return makeReadyResult( make_expected_success (zypp::RepoStatus() ));

              zypp::RepoStatus status ( repomdFile->file() );

              if ( !status.empty() && _ctx->repoInfo ().requireStatusWithMediaFile()) {
                return _ctx->zyppContext()->provider()->provide( _handle, "/media.1/media"  , ProvideFileSpec())
                  | [status = std::move(status)]( expected<ProvideRes> mediaFile ) mutable {
                      if ( mediaFile ) {
                        return make_expected_success( status && zypp::RepoStatus( mediaFile->file()) );
                      }
                      return make_expected_success( std::move(status) );
                    };
              }
              return makeReadyResult( make_expected_success(std::move(status)) );
            };
      }

      DlContextRefType _ctx;
      MediaHandle _handle;
    };
  }

  AsyncOpRef<expected<zypp::RepoStatus> > repoStatus(repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle)
  {
    return SimpleExecutor< StatusLogic, AsyncOp<expected<zypp::RepoStatus>> >::run( std::move(dl), std::move(mediaHandle) );
  }

  expected<zypp::RepoStatus> repoStatus(repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle)
  {
    return SimpleExecutor< StatusLogic, SyncOp<expected<zypp::RepoStatus>> >::run( std::move(dl), std::move(mediaHandle) );
  }


  namespace {

    using namespace zyppng::operators;

    template<class Executor, class OpType>
    struct DlLogic : public LogicBase<Executor, OpType>, private zypp::repo::yum::RepomdFileCollector {

      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);
    public:

      using DlContextRefType = std::conditional_t<zyppng::detail::is_async_op_v<OpType>, repo::AsyncDownloadContextRef, repo::SyncDownloadContextRef>;
      using ZyppContextType = typename DlContextRefType::element_type::ContextType;
      using ProvideType     = typename ZyppContextType::ProvideType;
      using MediaHandle     = typename ProvideType::MediaHandle;
      using ProvideRes      = typename ProvideType::Res;

      DlLogic( DlContextRefType ctx, MediaHandle &&mediaHandle, ProgressObserverRef &&progressObserver )
        : zypp::repo::yum::RepomdFileCollector( ctx->destDir() )
        , _ctx( std::move(ctx))
        , _mediaHandle(std::move(mediaHandle))
        , _progressObserver(std::move(progressObserver))
      {}

      auto execute() {
        // download media file here
        return RepoDownloaderWorkflow::downloadMediaInfo( _mediaHandle, _ctx->destDir() )
          | [this]( expected<zypp::ManagedFile> &&mediaInfo ) {

              // remember the media info if we had one
              if ( mediaInfo ) _ctx->files().push_back ( std::move(mediaInfo.get()) );

              if ( _progressObserver ) _progressObserver->inc();

              return RepoDownloaderWorkflow::downloadMasterIndex( _ctx, _mediaHandle, _ctx->repoInfo().path() / "/repodata/repomd.xml" )
                | inspect( incProgress( _progressObserver ) )
                | and_then( [this] ( DlContextRefType && ) {

                    zypp::Pathname repomdPath = _ctx->files().front();
                    std::vector<zypp::OnMediaLocation> requiredFiles;
                    try {
                      zypp::parser::yum::RepomdFileReader reader( repomdPath, [this]( const zypp::OnMediaLocation & loc_r, const std::string & typestr_r ){ return collect( loc_r, typestr_r ); });
                      finalize([&]( const zypp::OnMediaLocation &file ){
                        if ( file.medianr () != 1 ) {
                          // ALL repo files NEED to come from media nr 1 , otherwise we fail
                          ZYPP_THROW(zypp::repo::RepoException( _ctx->repoInfo(), "Repo can only require metadata files from primary medium."));
                        }
                        requiredFiles.push_back( file );
                      });
                    } catch ( ... ) {
                      return makeReadyResult(expected<DlContextRefType>::error( ZYPP_FWD_CURRENT_EXCPT() ) );
                    }

                    // add the required files to the base steps
                    if ( _progressObserver ) _progressObserver->setBaseSteps ( _progressObserver->baseSteps () + requiredFiles.size() );

                    return transform_collect  ( std::move(requiredFiles), [this]( zypp::OnMediaLocation file ) {

                      return DownloadWorkflow::provideToCacheDir( _ctx, _mediaHandle, file.filename(), ProvideFileSpec(file) )
                          | inspect ( incProgress( _progressObserver ) );

                    }) | and_then ( [this]( std::vector<zypp::ManagedFile> &&dlFiles ) {
                      auto &downloadedFiles = _ctx->files();
                      downloadedFiles.insert( downloadedFiles.end(), std::make_move_iterator(dlFiles.begin()), std::make_move_iterator(dlFiles.end()) );
                      return expected<DlContextRefType>::success( std::move(_ctx) );
                    });
                  });

          } | finishProgress( _progressObserver );
      }

    private:

      const zypp::RepoInfo &repoInfo() const override {
        return _ctx->repoInfo();
      }

      const zypp::filesystem::Pathname &deltaDir() const override {
        return _ctx->deltaDir();
      }

      DlContextRefType _ctx;
      MediaHandle _mediaHandle;
      ProgressObserverRef _progressObserver;
    };
  }

  AsyncOpRef<expected<repo::AsyncDownloadContextRef> > download(repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver)
  {
    return SimpleExecutor< DlLogic, AsyncOp<expected<repo::AsyncDownloadContextRef>> >::run( std::move(dl), std::move(mediaHandle), std::move(progressObserver) );
  }

  expected<repo::SyncDownloadContextRef> download(repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle, ProgressObserverRef progressObserver)
  {
    return SimpleExecutor< DlLogic, SyncOp<expected<repo::SyncDownloadContextRef>> >::run( std::move(dl), std::move(mediaHandle), std::move(progressObserver) );
  }


}
