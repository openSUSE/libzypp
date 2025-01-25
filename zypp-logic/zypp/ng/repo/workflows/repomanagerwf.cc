/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "repomanagerwf.h"

#include "zypp/parser/xml/Reader.h"

#include <zypp-core/ManagedFile.h>
#include <zypp-core/MirroredOrigin.h>
#include <zypp-core/ng/io/Process>
#include <zypp-core/ng/pipelines/MTry>
#include <zypp-core/ng/pipelines/Algorithm>
#include <zypp-media/MediaException>
#include <zypp/ng/media/Provide>
#include <zypp-media/ng/ProvideSpec>

#include <zypp-core/ExternalProgram.h>
#include <zypp/HistoryLog.h>
#include <zypp/base/Algorithm.h>
#include <zypp/ng/Context>
#include <zypp/ng/workflows/logichelpers.h>
#include <zypp/ng/repo/workflows/repodownloaderwf.h>
#include <zypp/ng/repomanager.h>
#include <zypp/ZConfig.h>

#include <utility>
#include <fstream>

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::repomanager"

namespace zyppng::RepoManagerWorkflow {

  using namespace zyppng::operators;

  namespace {

  template <class Executor, class OpType>
  struct ProbeRepoLogic : public LogicBase<Executor, OpType>
  {
  protected:
    ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

  public:
    using MediaHandle     = typename Provide::MediaHandle;
    using LazyMediaHandle = typename Provide::LazyMediaHandle;
    using ProvideRes      = typename Provide::Res;

    ProbeRepoLogic(ContextRef zyppCtx, LazyMediaHandle &&medium, zypp::Pathname &&path, std::optional<zypp::Pathname> &&targetPath )
      : _zyppContext(std::move(zyppCtx))
      , _medium(std::move(medium))
      , _path(std::move(path))
      , _targetPath(std::move(targetPath))
    {}

    MaybeAsyncRef<expected<zypp::repo::RepoType>> execute( ) {
      const auto &url = _medium.baseUrl();
      MIL << "going to probe the repo type at " << url << " (" << _path << ")" << std::endl;

      if ( url.getScheme() == "dir" && ! zypp::PathInfo( url.getPathName()/_path ).isDir() ) {
        // Handle non existing local directory in advance
        MIL << "Probed type NONE (not exists) at " << url << " (" << _path << ")" << std::endl;
        return makeReadyResult(expected<zypp::repo::RepoType>::success(zypp::repo::RepoType::NONE));
      }

      // prepare exception to be thrown if the type could not be determined
      // due to a media exception. We can't throw right away, because of some
      // problems with proxy servers returning an incorrect error
      // on ftp file-not-found(bnc #335906). Instead we'll check another types
      // before throwing.

      std::shared_ptr<Provide> providerRef = _zyppContext->provider();

      // TranslatorExplanation '%s' is an URL
      _error = zypp::repo::RepoException (zypp::str::form( _("Error trying to read from '%s'"), url.asString().c_str() ));

      return providerRef->attachMediaIfNeeded( _medium )
      | and_then([this, providerRef]( MediaHandle medium )
      {
        // first try rpmmd
        return providerRef->provide( medium, _path/"repodata/repomd.xml", ProvideFileSpec().setCheckExistsOnly( !_targetPath.has_value() ).setMirrorsAllowed(false) )
          | and_then( maybeCopyResultToDest("repodata/repomd.xml") )
          | and_then( [](){ return expected<zypp::repo::RepoType>::success(zypp::repo::RepoType::RPMMD); } )
          // try susetags if rpmmd fails and remember the error
          | or_else( [this, providerRef, medium]( std::exception_ptr err ) {
            try {
              std::rethrow_exception (err);
            } catch ( const zypp::media::MediaFileNotFoundException &e ) {
              // do nothing
              ;
            } catch( const zypp::media::MediaException &e ) {
              DBG << "problem checking for repodata/repomd.xml file" << std::endl;
              _error.remember ( err );
              _gotMediaError = true;
            } catch( ... ) {
              // any other error, we give up
              return makeReadyResult( expected<zypp::repo::RepoType>::error( ZYPP_FWD_CURRENT_EXCPT() ) );
            }
            return providerRef->provide( medium, _path/"content", ProvideFileSpec().setCheckExistsOnly( !_targetPath.has_value() ).setMirrorsAllowed(false) )
                | and_then( maybeCopyResultToDest("content") )
                | and_then( []()->expected<zypp::repo::RepoType>{ return expected<zypp::repo::RepoType>::success(zypp::repo::RepoType::YAST2); } );
          })
          // no rpmmd and no susetags!
          | or_else( [this, medium]( std::exception_ptr err ) {

            try {
              std::rethrow_exception (err);
            } catch ( const zypp::media::MediaFileNotFoundException &e ) {
              // do nothing
              ;
            } catch( const zypp::media::MediaException &e ) {
              DBG << "problem checking for content file" << std::endl;
              _error.remember ( err );
              _gotMediaError = true;
            } catch( zypp::Exception &e ) {
              _error.remember(e);
              // any other error, we give up
              return expected<zypp::repo::RepoType>::error( ZYPP_EXCPT_PTR(_error) );
            } catch(...) {
              // any other error, we give up
              return expected<zypp::repo::RepoType>::error( ZYPP_FWD_CURRENT_EXCPT() );
            }

            const auto &url = medium.baseUrl();

            // if it is a non-downloading URL denoting a directory (bsc#1191286: and no plugin)
            if ( ! ( url.schemeIsDownloading() || url.schemeIsPlugin() ) ) {

              if ( medium.localPath() && zypp::PathInfo(medium.localPath().value()/_path).isDir() ) {
                // allow empty dirs for now
                MIL << "Probed type RPMPLAINDIR at " << url << " (" << _path << ")" << std::endl;
                return expected<zypp::repo::RepoType>::success(zypp::repo::RepoType::RPMPLAINDIR);
              }
            }

            if( _gotMediaError )
              return expected<zypp::repo::RepoType>::error( ZYPP_EXCPT_PTR( _error ));

            MIL << "Probed type NONE at " << url << " (" << _path << ")" << std::endl;
            return expected<zypp::repo::RepoType>::success(zypp::repo::RepoType::NONE);
          })
        ;
      });
    }

  private:
    /**
     * creates a callback that copies the downloaded file into the target directory if it was given.
     * \returns expected<void>::success() if everything worked out
     */
    auto maybeCopyResultToDest ( std::string &&subPath ) {
      return [this, subPath = std::move(subPath)]( ProvideRes file ) -> MaybeAsyncRef<expected<void>> {
        if ( _targetPath ) {
          MIL << "Target path is set, copying " << file.file() << " to " << *_targetPath/subPath << std::endl;
          return std::move(file)
              | Provide::copyResultToDest( _zyppContext->provider(), *_targetPath/subPath)
              | and_then([]( zypp::ManagedFile file ){ file.resetDispose(); return expected<void>::success(); } );
        }
        return makeReadyResult( expected<void>::success() );
      };
    }

  private:
    ContextRef _zyppContext;
    LazyMediaHandle _medium;
    zypp::Pathname _path;
    std::optional<zypp::Pathname> _targetPath;

    zypp::repo::RepoException _error;
    bool _gotMediaError = false;
  };

  auto probeRepoLogic( ContextRef ctx, RepoInfo repo, std::optional<zypp::Pathname> targetPath)
  {
    using namespace zyppng::operators;
    return ctx->provider()->prepareMedia( repo.url(), zyppng::ProvideMediaSpec() )
      | and_then( [ctx, path = repo.path() ]( auto &&mediaHandle ) {
        return probeRepoType( ctx, std::forward<decltype(mediaHandle)>(mediaHandle), path );
    });
  }

  }

  MaybeAwaitable<expected<zypp::repo::RepoType> > probeRepoType( ContextRef ctx, Provide::LazyMediaHandle medium, zypp::Pathname path, std::optional<zypp::Pathname> targetPath)
  {
    if constexpr (ZYPP_IS_ASYNC)
      return SimpleExecutor< ProbeRepoLogic, AsyncOp<expected<zypp::repo::RepoType>> >::run( std::move(ctx), std::move(medium), std::move(path), std::move(targetPath) );
    else
      return SimpleExecutor< ProbeRepoLogic, SyncOp<expected<zypp::repo::RepoType>> >::run( std::move(ctx), std::move(medium), std::move(path), std::move(targetPath) );
  }

  MaybeAwaitable<expected<zypp::repo::RepoType> > probeRepoType( ContextRef ctx, RepoInfo repo, std::optional<zypp::Pathname> targetPath )
  {
    if constexpr ( ZYPP_IS_ASYNC )
      return probeRepoLogic( std::move(ctx), std::move(repo), std::move(targetPath) );
    else
      return probeRepoLogic( std::move(ctx), std::move(repo), std::move(targetPath) );
  }


  namespace {
    auto readRepoFileLogic( ContextRef ctx, zypp::Url repoFileUrl )
    {
      using namespace zyppng::operators;
      return ctx->provider()->provide( repoFileUrl, ProvideFileSpec() )
      | and_then([repoFileUrl]( auto local ){
        DBG << "reading repo file " << repoFileUrl << ", local path: " << local.file() << std::endl;
        return repositories_in_file( local.file() );
      });
    }
  }

  MaybeAwaitable<expected<std::list<RepoInfo> > > readRepoFile(ContextRef ctx, zypp::Url repoFileUrl)
  {
    return readRepoFileLogic( std::move(ctx), std::move(repoFileUrl) );
  }

  namespace {

    template<typename Executor, class OpType>
    struct CheckIfToRefreshMetadataLogic : public LogicBase<Executor, OpType> {

      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);
    public:
      using LazyMediaHandle = typename Provide::LazyMediaHandle;
      using MediaHandle     = typename Provide::MediaHandle;
      using ProvideRes      = typename Provide::Res;

      CheckIfToRefreshMetadataLogic( repo::RefreshContextRef refCtx, LazyMediaHandle &&medium, ProgressObserverRef progressObserver )
        : _refreshContext(std::move(refCtx))
        , _progress(std::move( progressObserver ))
        , _medium(std::move( medium ))
      {}

      MaybeAsyncRef<expected<repo::RefreshCheckStatus>> execute( ) {

        MIL << "Going to CheckIfToRefreshMetadata" << std::endl;

        return assert_alias( _refreshContext->repoInfo() )
        | and_then( [this] {

          const auto &info = _refreshContext->repoInfo();
          MIL << "Check if to refresh repo " << _refreshContext->repoInfo().alias() << " at " << _medium.baseUrl() << " (" << info.type() << ")" << std::endl;

          // first check old (cached) metadata
          return zyppng::RepoManager::metadataStatus( info, _refreshContext->repoManagerOptions() );
        })
        | and_then( [this](zypp::RepoStatus oldstatus) {

          const auto &info = _refreshContext->repoInfo();

          if ( oldstatus.empty() ) {
            MIL << "No cached metadata, going to refresh" << std::endl;
            return makeReadyResult( expected<repo::RefreshCheckStatus>::success(zypp::RepoManagerFlags::REFRESH_NEEDED) );
          }

          if ( _medium.baseUrl().schemeIsVolatile() ) {
            MIL << "Never refresh CD/DVD" << std::endl;
            return makeReadyResult( expected<repo::RefreshCheckStatus>::success(zypp::RepoManagerFlags::REPO_UP_TO_DATE) );
          }

          if ( _refreshContext->policy() == zypp::RepoManagerFlags::RefreshForced ) {
            MIL << "Forced refresh!" << std::endl;
            return makeReadyResult( expected<repo::RefreshCheckStatus>::success(zypp::RepoManagerFlags::REFRESH_NEEDED) );
          }

          if ( _medium.baseUrl().schemeIsLocal() ) {
            _refreshContext->setPolicy( zypp::RepoManagerFlags::RefreshIfNeededIgnoreDelay );
          }

          // Check whether repo.refresh.delay applies...
          if ( _refreshContext->policy() != zypp::RepoManagerFlags::RefreshIfNeededIgnoreDelay )
          {
            // bsc#1174016: Prerequisite to skipping the refresh is that metadata
            // and solv cache status match. They will not, if the repos URL was
            // changed e.g. due to changed repovars.
            expected<zypp::RepoStatus> cachestatus = zyppng::RepoManager::cacheStatus( info, _refreshContext->repoManagerOptions() );
            if ( !cachestatus ) return makeReadyResult( expected<repo::RefreshCheckStatus>::error(cachestatus.error()) );

            if ( oldstatus == *cachestatus ) {
              // difference in seconds
              double diff = ::difftime( (zypp::Date::ValueType)zypp::Date::now(), (zypp::Date::ValueType)oldstatus.timestamp() ) / 60;
              const auto refDelay = _refreshContext->zyppContext()->config().repo_refresh_delay();
              if ( diff < refDelay ) {
                if ( diff < 0 ) {
                  WAR << "Repository '" << info.alias() << "' was refreshed in the future!" << std::endl;
                }
                else {
                  MIL << "Repository '" << info.alias()
                  << "' has been refreshed less than repo.refresh.delay ("
                  << refDelay
                  << ") minutes ago. Advising to skip refresh" << std::endl;
                  return makeReadyResult( expected<repo::RefreshCheckStatus>::success(zypp::RepoManagerFlags::REPO_CHECK_DELAYED) );
                }
              }
            }
            else {
              MIL << "Metadata and solv cache don't match. Check data on server..." << std::endl;
            }
          }

          return info.type() | [this]( zypp::repo::RepoType repokind ) {
            // if unknown: probe it
            if ( repokind == zypp::repo::RepoType::NONE )
              return probeRepoType( _refreshContext->zyppContext(), _medium, _refreshContext->repoInfo().path()/*, _refreshContext->targetDir()*/ );
            return makeReadyResult( expected<zypp::repo::RepoType>::success(repokind) );
          } | and_then([this, oldstatus]( zypp::repo::RepoType repokind ) {

            // make sure to remember the repo type
            _refreshContext->repoInfo().setProbedType( repokind );

            auto dlContext = std::make_shared<repo::DownloadContext>( _refreshContext->zyppContext(), _refreshContext->repoInfo(), _refreshContext->targetDir() );
            return RepoDownloaderWorkflow::repoStatus ( dlContext, _medium )
              | and_then( [this, dlContext, oldstatus]( zypp::RepoStatus newstatus ){
                // check status
                if ( oldstatus == newstatus ) {
                  MIL << "repo has not changed" << std::endl;
                  return zyppng::RepoManager::touchIndexFile( _refreshContext->repoInfo(), _refreshContext->repoManagerOptions() )
                  | and_then([](){ return expected<repo::RefreshCheckStatus>::success(zypp::RepoManagerFlags::REPO_UP_TO_DATE); });
                }
                else { // includes newstatus.empty() if e.g. repo format changed
                  MIL << "repo has changed, going to refresh" << std::endl;
                  MIL << "Old status: " << oldstatus << " New Status: " << newstatus << std::endl;
                  return expected<repo::RefreshCheckStatus>::success(zypp::RepoManagerFlags::REFRESH_NEEDED);
                }
              });
          });
        });
      }

    protected:
      repo::RefreshContextRef _refreshContext;
      ProgressObserverRef _progress;
      LazyMediaHandle _medium;
    };
  }

  MaybeAwaitable<expected<repo::RefreshCheckStatus> > checkIfToRefreshMetadata(repo::RefreshContextRef refCtx, LazyMediaHandle<Provide> medium, ProgressObserverRef progressObserver)
  {
    if constexpr ( ZYPP_IS_ASYNC )
      return SimpleExecutor< CheckIfToRefreshMetadataLogic , AsyncOp<expected<repo::RefreshCheckStatus>> >::run( std::move(refCtx), std::move(medium), std::move(progressObserver) );
    else
      return SimpleExecutor< CheckIfToRefreshMetadataLogic , SyncOp<expected<repo::RefreshCheckStatus>> >::run( std::move(refCtx), std::move(medium), std::move(progressObserver) );
  }


  namespace {

    template<typename Executor, class OpType>
    struct RefreshMetadataLogic : public LogicBase<Executor, OpType>{

      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

    public:
      using MediaHandle        = typename Provide::MediaHandle;
      using LazyMediaHandle    = typename Provide::LazyMediaHandle;
      using ProvideRes         = typename Provide::Res;

      using DlContextType    = repo::DownloadContext;
      using DlContextRefType = std::shared_ptr<DlContextType>;

      RefreshMetadataLogic( repo::RefreshContextRef refCtx, LazyMediaHandle &&medium, ProgressObserverRef progressObserver )
        : _refreshContext(std::move(refCtx))
        , _progress ( std::move( progressObserver ) )
        , _medium   ( std::move( medium ) )
      { }

      MaybeAsyncRef<expected<repo::RefreshContextRef>> execute() {

        return assert_alias( _refreshContext->repoInfo() )
        | and_then( [this](){ return assert_urls( _refreshContext->repoInfo() ); })
        | and_then( [this](){ return checkIfToRefreshMetadata ( _refreshContext, _medium, _progress ); })
        | and_then( [this]( repo::RefreshCheckStatus status ){

          MIL << "RefreshCheckStatus returned: " << status << std::endl;

          // check whether to refresh metadata
          // if the check fails for this url, it throws, so another url will be checked
          if ( status != zypp::RepoManagerFlags::REFRESH_NEEDED )
            return makeReadyResult ( expected<repo::RefreshContextRef>::success( std::move(_refreshContext) ) );

          // if REFRESH_NEEDED but we don't have the permission to write the cache, stop here.
          if ( zypp::IamNotRoot() && not zypp::PathInfo(_refreshContext->rawCachePath().dirname()).userMayWX() ) {
            WAR << "No permision to write cache " << zypp::PathInfo(_refreshContext->rawCachePath().dirname()) << std::endl;
            auto exception = ZYPP_EXCPT_PTR( zypp::repo::RepoNoPermissionException( _refreshContext->repoInfo() ) );
            return makeReadyResult( expected<repo::RefreshContextRef>::error( std::move(exception) ) );
          }

          MIL << "Going to refresh metadata from " << _medium.baseUrl() << std::endl;

          // bsc#1048315: Always re-probe in case of repo format change.
          // TODO: Would be sufficient to verify the type and re-probe
          // if verification failed (or type is RepoType::NONE)
          return probeRepoType ( _refreshContext->zyppContext(), _medium, _refreshContext->repoInfo().path() /*, _refreshContext->targetDir()*/ )
          | and_then([this]( zypp::repo::RepoType repokind ) {

            auto &info = _refreshContext->repoInfo();

            if ( info.type() != repokind ) {
              _refreshContext->setProbedType( repokind );
              // Adjust the probed type in RepoInfo
              info.setProbedType( repokind ); // lazy init!
            }

            // no need to continue with an unknown type
            if ( repokind.toEnum() == zypp::repo::RepoType::NONE_e )
              return makeReadyResult( expected<DlContextRefType>::error( ZYPP_EXCPT_PTR ( zypp::repo::RepoUnknownTypeException( info ))) );

            const zypp::Pathname &mediarootpath = _refreshContext->rawCachePath();
            if( zypp::filesystem::assert_dir(mediarootpath) ) {
              auto exception = ZYPP_EXCPT_PTR (zypp::Exception(zypp::str::form( _("Can't create %s"), mediarootpath.c_str() )));
              return makeReadyResult( expected<DlContextRefType>::error( std::move(exception) ));
            }

            auto dlContext = std::make_shared<DlContextType>( _refreshContext->zyppContext(), _refreshContext->repoInfo(), _refreshContext->targetDir() );
            dlContext->setPluginRepoverification( _refreshContext->pluginRepoverification() );

            return RepoDownloaderWorkflow::download ( dlContext, _medium, _progress );

          })
          | and_then([this]( DlContextRefType && ) {

            // ok we have the metadata, now exchange
            // the contents
            _refreshContext->saveToRawCache();
            // if ( ! isTmpRepo( info ) )
            //  reposManip();	// remember to trigger appdata refresh

            // we are done.
            return expected<repo::RefreshContextRef>::success( std::move(_refreshContext) );
          });
        });
      }

      repo::RefreshContextRef _refreshContext;
      ProgressObserverRef _progress;
      LazyMediaHandle _medium;
      zypp::Pathname _mediarootpath;

    };
  }

  MaybeAwaitable<expected<repo::RefreshContextRef> > refreshMetadata( repo::RefreshContextRef refCtx, LazyMediaHandle<Provide> medium, ProgressObserverRef progressObserver )
  {
    if constexpr (ZYPP_IS_ASYNC)
      return SimpleExecutor<RefreshMetadataLogic, AsyncOp<expected<repo::RefreshContextRef>>>::run( std::move(refCtx), std::move(medium), std::move(progressObserver));
    else
      return SimpleExecutor<RefreshMetadataLogic, SyncOp<expected<repo::RefreshContextRef>>>::run( std::move(refCtx), std::move(medium), std::move(progressObserver));
  }

  namespace {
    auto refreshMetadataLogic( repo::RefreshContextRef refCtx, ProgressObserverRef progressObserver)
    {
      // small shared helper struct to pass around the exception and to remember that we tried the first URL
      struct ExHelper
      {
        // We will throw this later if no URL checks out fine.
        // The first exception will be remembered, further exceptions just added to the history.
        ExHelper( const RepoInfo & info_r )
        : rexception { info_r, _("Failed to retrieve new repository metadata.") }
        {}
        void remember( const zypp::Exception & old_r )
        {
          if ( rexception.historyEmpty() ) {
            rexception.remember( old_r );
          } else {
            rexception.addHistory( old_r.asUserString() );
          }
        }
        zypp::repo::RepoException rexception;
      };

      auto helper = std::make_shared<ExHelper>( ExHelper{ refCtx->repoInfo() } );

      // the actual logic pipeline, attaches the medium and tries to refresh from it
      auto refreshPipeline = [ refCtx, progressObserver ]( zypp::MirroredOrigin origin ){
        return refCtx->zyppContext()->provider()->prepareMedia( origin, zyppng::ProvideMediaSpec() )
            | and_then( [ refCtx , progressObserver]( auto mediaHandle ) mutable { return refreshMetadata ( std::move(refCtx), std::move(mediaHandle), progressObserver ); } );
      };

      // predicate that accepts only valid results, and in addition collects all errors in rexception
      auto predicate = [ info = refCtx->repoInfo(), helper ]( const expected<repo::RefreshContextRef> &res ) -> bool{
        if ( !res ) {
          try {
            ZYPP_RETHROW( res.error() );
          } catch ( const zypp::repo::RepoNoPermissionException &e ) {
            // We deliver the Exception caught here (no permission to write chache) and give up.
            ERR << "Giving up..." << std::endl;
            helper->remember( e );
            return true;  // stop processing
          } catch ( const zypp::Exception &e ) {
            ERR << "Trying another url..." << std::endl;
            helper->remember( e );
          }
          return false;
        }
        return true;
      };


      // now go over the url groups until we find one that works
      return refCtx->repoInfo().repoOrigins()
        | firstOf( std::move(refreshPipeline), expected<repo::RefreshContextRef>::error( std::make_exception_ptr(NotFoundException()) ), std::move(predicate) )
        | [helper]( expected<repo::RefreshContextRef> result ) {
          if ( !result ) {
            // none of the URLs worked
            ERR << "No more urls..." << std::endl;
            return expected<repo::RefreshContextRef>::error( ZYPP_EXCPT_PTR(helper->rexception) );
          }
          // we are done.
          return result;
        };
    }
  }

  MaybeAwaitable<expected<repo::RefreshContextRef> > refreshMetadata( repo::RefreshContextRef refCtx, ProgressObserverRef progressObserver) {
    return refreshMetadataLogic ( std::move(refCtx), std::move(progressObserver) );
  }


  namespace {

#ifdef ZYPP_ENABLE_ASYNC
    struct Repo2SolvOp : public AsyncOp<expected<void>>
    {
      Repo2SolvOp() { }

      static AsyncOpRef<expected<void>> run( zypp::RepoInfo repo, zypp::ExternalProgram::Arguments args ) {
        MIL << "Starting repo2solv for repo " << repo.alias () << std::endl;
        auto me = std::make_shared<Repo2SolvOp<ContextRef>>();
        me->_repo = std::move(repo);
        me->_proc = Process::create();
        me->_proc->connect( &Process::sigFinished, *me, &Repo2SolvOp<ContextRef>::procFinished );
        me->_proc->connect( &Process::sigReadyRead, *me, &Repo2SolvOp<ContextRef>::readyRead );

        std::vector<const char *> argsIn;
        argsIn.reserve ( args.size() );
        std::for_each( args.begin (), args.end(), [&]( const std::string &s ) { argsIn.push_back(s.data()); });
        argsIn.push_back (nullptr);
        me->_proc->setOutputChannelMode ( Process::Merged );
        if (!me->_proc->start( argsIn.data() )) {
          return makeReadyResult( expected<void>::error(ZYPP_EXCPT_PTR(zypp::repo::RepoException ( me->_repo, _("Failed to cache repo ( unable to start repo2solv ).") ))) );
        }
        return me;
      }

      void readyRead (){
        const ByteArray &data = _proc->readLine();
        const std::string &line = data.asString();
        WAR << "  " << line;
        _errdetail += line;
      }

      void procFinished( int ret ) {

        while ( _proc->canReadLine() )
          readyRead();

        if ( ret != 0 ) {
          zypp::repo::RepoException ex( _repo, zypp::str::form( _("Failed to cache repo (%d)."), ret ));
          ex.addHistory( zypp::str::Str() << _proc->executedCommand() << std::endl << _errdetail << _proc->execError() ); // errdetail lines are NL-terminaled!
          setReady( expected<void>::error(ZYPP_EXCPT_PTR(ex)) );
          return;
        }
        setReady( expected<void>::success() );
      }

    private:
      ProcessRef  _proc;
      zypp::RepoInfo _repo;
      std::string _errdetail;
    };
#else
    struct Repo2SolvOp
    {
      static expected<void> run( zypp::RepoInfo repo, zypp::ExternalProgram::Arguments args ) {
        zypp::ExternalProgram prog( args, zypp::ExternalProgram::Stderr_To_Stdout );
        std::string errdetail;

        for ( std::string output( prog.receiveLine() ); output.length(); output = prog.receiveLine() ) {
          WAR << "  " << output;
          errdetail += output;
        }

        int ret = prog.close();
        if ( ret != 0 )
        {
          zypp::repo::RepoException ex(repo, zypp::str::form( _("Failed to cache repo (%d)."), ret ));
          ex.addHistory( zypp::str::Str() << prog.command() << std::endl << errdetail << prog.execError() ); // errdetail lines are NL-terminaled!
          return expected<void>::error(ZYPP_EXCPT_PTR(ex));
        }
        return expected<void>::success();
      }
    };
#endif

    template<typename Executor, class OpType>
    struct BuildCacheLogic : public LogicBase<Executor, OpType>{

      using MediaHandle           = typename Provide::MediaHandle;
      using ProvideRes            = typename Provide::Res;

      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

      BuildCacheLogic( repo::RefreshContextRef &&refCtx, zypp::RepoManagerFlags::CacheBuildPolicy policy, ProgressObserverRef &&progressObserver )
        : _refCtx( std::move(refCtx) )
        , _policy( policy )
        , _progressObserver( std::move(progressObserver) )
      {}

      MaybeAsyncRef<expected<repo::RefreshContextRef>> execute() {

        ProgressObserver::setup ( _progressObserver, zypp::str::form(_("Building repository '%s' cache"), _refCtx->repoInfo().label().c_str()), 100 );

        return assert_alias(_refCtx->repoInfo() )
        | and_then( mtry( [this] {
          _mediarootpath   = rawcache_path_for_repoinfo( _refCtx->repoManagerOptions(), _refCtx->repoInfo() ).unwrap();
          _productdatapath = rawproductdata_path_for_repoinfo( _refCtx->repoManagerOptions(), _refCtx->repoInfo() ).unwrap();
        }))
        | and_then( [this] {

          const auto &options = _refCtx->repoManagerOptions();

          if( zypp::filesystem::assert_dir( options.repoCachePath ) ) {
            auto ex = ZYPP_EXCPT_PTR( zypp::Exception (zypp::str::form( _("Can't create %s"), options.repoCachePath.c_str()) ) );
            return expected<RepoStatus>::error( std::move(ex) );
          }

          return RepoManager::metadataStatus( _refCtx->repoInfo(), options );

        }) | and_then( [this](RepoStatus raw_metadata_status ) {

          if ( raw_metadata_status.empty() )
          {
            // If there is no raw cache at this point, we refresh the raw metadata.
            // This may happen if no autorefresh is configured and no explicit
            // refresh was called.
            //
            zypp::Pathname mediarootParent { _mediarootpath.dirname() };

            if ( zypp::filesystem::assert_dir( mediarootParent ) == 0
              && ( zypp::IamRoot() || zypp::PathInfo(mediarootParent).userMayWX() ) ) {

              return refreshMetadata( _refCtx, ProgressObserver::makeSubTask( _progressObserver ) )
              | and_then([this]( auto /*refCtx*/) { return RepoManager::metadataStatus( _refCtx->repoInfo(), _refCtx->repoManagerOptions() ); } );

            } else {
              // Non-root user is not allowed to write the raw cache.
              WAR << "No permission to write raw cache " << mediarootParent << std::endl;
              auto exception = ZYPP_EXCPT_PTR( zypp::repo::RepoNoPermissionException( _refCtx->repoInfo() ) );
              return makeReadyResult( expected<zypp::RepoStatus>::error( std::move(exception) ) );
            }
          }
          return makeReadyResult( make_expected_success (raw_metadata_status) );

        }) | and_then( [this]( RepoStatus raw_metadata_status ) {

          bool needs_cleaning = false;
          const auto &info = _refCtx->repoInfo();
          if ( _refCtx->repoManager()->isCached( info ) )
          {
            MIL << info.alias() << " is already cached." << std::endl;
            expected<RepoStatus> cache_status = RepoManager::cacheStatus( info, _refCtx->repoManagerOptions() );
            if ( !cache_status )
              return makeReadyResult( expected<void>::error(cache_status.error()) );

            if ( *cache_status == raw_metadata_status )
            {
              MIL << info.alias() << " cache is up to date with metadata." << std::endl;
              if ( _policy == zypp::RepoManagerFlags::BuildIfNeeded )
              {
                // On the fly add missing solv.idx files for bash completion.
                return makeReadyResult(
                  solv_path_for_repoinfo( _refCtx->repoManagerOptions(), info)
                  | and_then([]( zypp::Pathname base ){
                    if ( ! zypp::PathInfo(base/"solv.idx").isExist() )
                      return mtry( zypp::sat::updateSolvFileIndex, base/"solv" );
                    return expected<void>::success ();
                  })
                );
              }
              else {
                MIL << info.alias() << " cache rebuild is forced" << std::endl;
              }
            }

            needs_cleaning = true;
          }

          ProgressObserver::start( _progressObserver );

          if (needs_cleaning)
          {
            auto r = _refCtx->repoManager()->cleanCache(info);
            if ( !r )
              return makeReadyResult( expected<void>::error(r.error()) );
          }

          MIL << info.alias() << " building cache..." << info.type() << std::endl;

          expected<zypp::Pathname> base = solv_path_for_repoinfo( _refCtx->repoManagerOptions(), info);
          if ( !base )
            return makeReadyResult( expected<void>::error(base.error()) );

          if( zypp::filesystem::assert_dir(*base) )
          {
            zypp::Exception ex(zypp::str::form( _("Can't create %s"), base->c_str()) );
            return makeReadyResult( expected<void>::error(ZYPP_EXCPT_PTR(ex)) );
          }

          if( zypp::IamNotRoot() && not zypp::PathInfo(*base).userMayW() )
          {
            zypp::Exception ex(zypp::str::form( _("Can't create cache at %s - no writing permissions."), base->c_str()) );
            return makeReadyResult( expected<void>::error(ZYPP_EXCPT_PTR(ex)) );
          }

          zypp::Pathname solvfile = *base / "solv";

          // do we have type?
          zypp::repo::RepoType repokind = info.type();

          // if the type is unknown, try probing.
          switch ( repokind.toEnum() )
          {
            case zypp::repo::RepoType::NONE_e:
              // unknown, probe the local metadata
              repokind = RepoManager::probeCache( _productdatapath );
            break;
            default:
            break;
          }

          MIL << "repo type is " << repokind << std::endl;

          return mountIfRequired( repokind, info )
          | and_then([this, repokind, solvfile = std::move(solvfile) ]( std::optional<MediaHandle> forPlainDirs ) mutable {

            const auto &info = _refCtx->repoInfo();

            switch ( repokind.toEnum() )
            {
              case zypp::repo::RepoType::RPMMD_e :
              case zypp::repo::RepoType::YAST2_e :
              case zypp::repo::RepoType::RPMPLAINDIR_e :
              {
                // Take care we unlink the solvfile on error
                zypp::ManagedFile guard( solvfile, zypp::filesystem::unlink );

                zypp::ExternalProgram::Arguments cmd;
#ifdef ZYPP_REPO2SOLV_PATH
                cmd.push_back( ZYPP_REPO2SOLV_PATH );
#else
                cmd.push_back( zypp::PathInfo( "/usr/bin/repo2solv" ).isFile() ? "repo2solv" : "repo2solv.sh" );
#endif
                // repo2solv expects -o as 1st arg!
                cmd.push_back( "-o" );
                cmd.push_back( solvfile.asString() );
                cmd.push_back( "-X" );	// autogenerate pattern from pattern-package
                // bsc#1104415: no more application support // cmd.push_back( "-A" );	// autogenerate application pseudo packages

                if ( repokind == zypp::repo::RepoType::RPMPLAINDIR )
                {
                  // recusive for plaindir as 2nd arg!
                  cmd.push_back( "-R" );

                  std::optional<zypp::Pathname> localPath = forPlainDirs.has_value() ? forPlainDirs->localPath() : zypp::Pathname();
                  if ( !localPath )
                    return makeReadyResult( expected<void>::error( ZYPP_EXCPT_PTR( zypp::repo::RepoException( zypp::str::Format(_("Failed to cache repo %1%")) % _refCtx->repoInfo() ))) );

                  // FIXME this does only work for dir: URLs
                  cmd.push_back( (*localPath / info.path().absolutename()).c_str() );
                }
                else
                  cmd.push_back( _productdatapath.asString() );

                return Repo2SolvOp::run( info, std::move(cmd) )
                | and_then( [guard = std::move(guard), solvfile = std::move(solvfile) ]() mutable {
                  // We keep it.
                  guard.resetDispose();
                  return mtry( zypp::sat::updateSolvFileIndex, solvfile ); // content digest for zypper bash completion
                });
              }
              break;
              default:
                return makeReadyResult( expected<void>::error( ZYPP_EXCPT_PTR(zypp::repo::RepoUnknownTypeException( info, _("Unhandled repository type") )) ) );
              break;
            }
          })
          | and_then([this, raw_metadata_status](){
            // update timestamp and checksum
            return _refCtx->repoManager()->setCacheStatus( _refCtx->repoInfo(), raw_metadata_status );
          });
        })
        | and_then( [this](){
          MIL << "Commit cache.." << std::endl;
          ProgressObserver::finish( _progressObserver, ProgressObserver::Success );
          return make_expected_success ( _refCtx );

        })
        | or_else ( [this]( std::exception_ptr e ) {
          ProgressObserver::finish( _progressObserver, ProgressObserver::Success );
          return expected<repo::RefreshContextRef>::error(e);
        });
      }

    private:
      MaybeAsyncRef<expected<std::optional<MediaHandle>>> mountIfRequired ( zypp::repo::RepoType repokind, zypp::RepoInfo info  ) {
        if ( repokind != zypp::repo::RepoType::RPMPLAINDIR )
          return makeReadyResult( make_expected_success( std::optional<MediaHandle>() ));

        return _refCtx->zyppContext()->provider()->attachMedia( info.url(), ProvideMediaSpec() )
        | and_then( []( MediaHandle handle ) {
          return makeReadyResult( make_expected_success( std::optional<MediaHandle>( std::move(handle)) ));
        });
      }

    private:
      repo::RefreshContextRef _refCtx;
      zypp::RepoManagerFlags::CacheBuildPolicy _policy;
      ProgressObserverRef   _progressObserver;

      zypp::Pathname _mediarootpath;
      zypp::Pathname _productdatapath;
    };
  }

  MaybeAwaitable<expected<repo::RefreshContextRef> > buildCache(repo::RefreshContextRef refCtx, zypp::RepoManagerFlags::CacheBuildPolicy policy, ProgressObserverRef progressObserver)
  {
    if constexpr ( ZYPP_IS_ASYNC )
      return SimpleExecutor<BuildCacheLogic, AsyncOp<expected<repo::RefreshContextRef>>>::run( std::move(refCtx), policy, std::move(progressObserver));
    else
      return SimpleExecutor<BuildCacheLogic, SyncOp<expected<repo::RefreshContextRef>>>::run( std::move(refCtx), policy, std::move(progressObserver));
  }


  // Add repository logic
  namespace {

    template<typename Executor, class OpType>
    struct AddRepoLogic : public LogicBase<Executor, OpType>{

      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

      AddRepoLogic( RepoManagerRef &&repoMgrRef, RepoInfo &&info, ProgressObserverRef &&myProgress, const zypp::TriBool & forcedProbe )
        : _repoMgrRef( std::move(repoMgrRef) )
        , _doProbeUrl( zypp::indeterminate(forcedProbe) ? _repoMgrRef->options().probe : bool(forcedProbe) )
        , _info( std::move(info) )
        , _myProgress ( std::move(myProgress) )
      {}

      MaybeAsyncRef<expected<RepoInfo> > execute() {
        using namespace zyppng::operators;

        return  assert_alias(_info)
        | and_then([this]( ) {

          MIL << "Try adding repo " << _info << std::endl;
          ProgressObserver::setup( _myProgress, zypp::str::form(_("Adding repository '%s'"), _info.label().c_str()) );
          ProgressObserver::start( _myProgress );

          if ( _repoMgrRef->repos().find(_info) != _repoMgrRef->repos().end() )
            return makeReadyResult( expected<RepoInfo>::error( ZYPP_EXCPT_PTR(zypp::repo::RepoAlreadyExistsException(_info)) ) );

          // check the first url for now
          if ( _doProbeUrl )
          {
            DBG << "unknown repository type, probing" << std::endl;
            return assert_urls(_info)
            | and_then([this]{ return probeRepoType( _repoMgrRef->zyppContext(), _info ); })
            | and_then([this]( zypp::repo::RepoType probedtype ) {

              if ( probedtype == zypp::repo::RepoType::NONE )
                return expected<RepoInfo>::error( ZYPP_EXCPT_PTR(zypp::repo::RepoUnknownTypeException(_info)) );

              RepoInfo tosave = _info;
              tosave.setType(probedtype);
              return make_expected_success(tosave);
            });
          }
          return makeReadyResult( make_expected_success(_info) );
        })
        | inspect( operators::setProgress( _myProgress, 50 ) )
        | and_then( [this]( RepoInfo tosave ){ return _repoMgrRef->addProbedRepository( tosave, tosave.type() ); })
        | and_then( [this]( RepoInfo updated ) {
          ProgressObserver::finish( _myProgress );
          MIL << "done" << std::endl;
          return expected<RepoInfo>::success( updated );
        })
        | or_else( [this]( std::exception_ptr e) {
          ProgressObserver::finish ( _myProgress, ProgressObserver::Error );
          MIL << "done" << std::endl;
          return expected<RepoInfo>::error(e);
        })
        ;
      }

      RepoManagerRef _repoMgrRef;
      bool _doProbeUrl;  ///< RepoManagerOptions::probe opt. overwritten in by ctor arg \a forcedProbe
      RepoInfo _info;
      ProgressObserverRef _myProgress;
    };
  };

  MaybeAwaitable<expected<RepoInfo> > addRepository( RepoManagerRef mgr, RepoInfo info, ProgressObserverRef myProgress, const zypp::TriBool & forcedProbe )
  {
    if constexpr ( ZYPP_IS_ASYNC )
      return SimpleExecutor<AddRepoLogic, AsyncOp<expected<RepoInfo>>>::run( std::move(mgr), std::move(info), std::move(myProgress), forcedProbe );
    else
      return SimpleExecutor<AddRepoLogic, SyncOp<expected<RepoInfo>>>::run( std::move(mgr), RepoInfo(info), std::move(myProgress), forcedProbe );
  }

  namespace {

    template<typename Executor, class OpType>
    struct AddReposLogic : public LogicBase<Executor, OpType>{
      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

      AddReposLogic( RepoManagerRef &&repoMgrRef, zypp::Url &&url, ProgressObserverRef &&myProgress )
        : _repoMgrRef( std::move(repoMgrRef) )
        , _url( std::move(url) )
        , _myProgress ( std::move(myProgress) )
      {}

      MaybeAsyncRef<expected<void>> execute() {
        using namespace zyppng::operators;

        return mtry( zypp::repo::RepoVariablesUrlReplacer(), _url )
        | and_then([this]( zypp::Url repoFileUrl ) { return readRepoFile( _repoMgrRef->zyppContext(), std::move(repoFileUrl) ); } )
        | and_then([this]( std::list<RepoInfo> repos ) {

          for ( std::list<RepoInfo>::const_iterator it = repos.begin();
                it != repos.end();
                ++it )
          {
            // look if the alias is in the known repos.
            for_ ( kit, _repoMgrRef->repoBegin(), _repoMgrRef->repoEnd() )
            {
              if ( (*it).alias() == (*kit).alias() )
              {
                ERR << "To be added repo " << (*it).alias() << " conflicts with existing repo " << (*kit).alias() << std::endl;
                return expected<void>::error(ZYPP_EXCPT_PTR(zypp::repo::RepoAlreadyExistsException(*it)));
              }
            }
          }

          std::string filename = zypp::Pathname(_url.getPathName()).basename();
          if ( filename == zypp::Pathname() )
          {
            // TranslatorExplanation '%s' is an URL
            return expected<void>::error(ZYPP_EXCPT_PTR(zypp::repo::RepoException(zypp::str::form( _("Invalid repo file name at '%s'"), _url.asString().c_str() ))));
          }

          const auto &options = _repoMgrRef->options();

          // assert the directory exists
          zypp::filesystem::assert_dir( options.knownReposPath );

          zypp::Pathname repofile = _repoMgrRef->generateNonExistingName( options.knownReposPath, filename );
          // now we have a filename that does not exists
          MIL << "Saving " << repos.size() << " repo" << ( repos.size() ? "s" : "" ) << " in " << repofile << std::endl;

          std::ofstream file(repofile.c_str());
          if (!file)
          {
            // TranslatorExplanation '%s' is a filename
            return expected<void>::error(ZYPP_EXCPT_PTR( zypp::Exception(zypp::str::form( _("Can't open file '%s' for writing."), repofile.c_str() ))));
          }

          for ( std::list<RepoInfo>::iterator it = repos.begin();
                it != repos.end();
                ++it )
          {
            MIL << "Saving " << (*it).alias() << std::endl;

            const auto &rawCachePath = rawcache_path_for_repoinfo( options, *it );
            if ( !rawCachePath ) return expected<void>::error(rawCachePath.error());

            const auto &pckCachePath = packagescache_path_for_repoinfo( options, *it ) ;
            if ( !pckCachePath ) return expected<void>::error(pckCachePath.error());

            it->dumpAsIniOn(file);
            it->setFilepath(repofile);
            it->setMetadataPath( *rawCachePath );
            it->setPackagesPath( *pckCachePath );
            _repoMgrRef->reposManip().insert(*it);

            zypp::HistoryLog( _repoMgrRef->options().rootDir).addRepository(*it);
          }

          MIL << "done" << std::endl;
          return expected<void>::success();
        });
      }

    private:
      RepoManagerRef _repoMgrRef;
      zypp::Url _url;
      ProgressObserverRef _myProgress;
    };

  }

  MaybeAwaitable<expected<void>> addRepositories( RepoManagerRef mgr, zypp::Url url, ProgressObserverRef myProgress )
  {
    if constexpr ( ZYPP_IS_ASYNC )
      return SimpleExecutor<AddReposLogic, AsyncOp<expected<void>>>::run( std::move(mgr), std::move(url), std::move(myProgress) );
    else
      return SimpleExecutor<AddReposLogic, SyncOp<expected<void>>>::run( std::move(mgr), std::move(url), std::move(myProgress) );
  }


  namespace {

    template<typename Executor, class OpType>
    struct RefreshGeoIpLogic : public LogicBase<Executor, OpType>{
      protected:
        ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

      public:
        using MediaHandle        = typename Provide::MediaHandle;
        using ProvideRes         = typename Provide::Res;


        RefreshGeoIpLogic( ContextRef &&zyppCtx, zypp::MirroredOriginSet &&origins )
          : _zyppCtx( std::move(zyppCtx) )
          , _origins( std::move(origins) )
        { }

        MaybeAsyncRef<expected<void>> execute() {

          using namespace zyppng::operators;

          if ( !_zyppCtx->config().geoipEnabled() ) {
            MIL << "GeoIp disabled via ZConfig, not refreshing the GeoIP information." << std::endl;
            return makeReadyResult(expected<void>::success());
          }

          std::vector<std::string> hosts;
          for ( const zypp::MirroredOrigin &origin : _origins ){
            if ( !origin.schemeIsDownloading () )
              continue;

            for ( const auto &originEndpoint : origin ) {
              const auto &host = originEndpoint.url().getHost();
              if ( zypp::any_of( _zyppCtx->config().geoipHostnames(), [&host]( const auto &elem ){ return ( zypp::str::compareCI( host, elem ) == 0 ); } ) ) {
                hosts.push_back( host );
                break;
              }
            }
          }

          if ( hosts.empty() ) {
            MIL << "No configured geoip URL found, not updating geoip data" << std::endl;
            return makeReadyResult(expected<void>::success());
          }

          _geoIPCache = _zyppCtx->config().geoipCachePath();

          if ( zypp::filesystem::assert_dir( _geoIPCache ) != 0 ) {
            MIL << "Unable to create cache directory for GeoIP." << std::endl;
            return makeReadyResult(expected<void>::success());
          }

          if ( zypp::IamNotRoot() && not zypp::PathInfo(_geoIPCache).userMayRWX() ) {
            MIL << "No access rights for the GeoIP cache directory." << std::endl;
            return  makeReadyResult(expected<void>::success());
          }

          // remove all older cache entries
          zypp::filesystem::dirForEachExt( _geoIPCache, []( const zypp::Pathname &dir, const zypp::filesystem::DirEntry &entry ) {
            if ( entry.type != zypp::filesystem::FT_FILE )
              return true;

            zypp::PathInfo pi( dir/entry.name );
            auto age = std::chrono::system_clock::now() - std::chrono::system_clock::from_time_t( pi.mtime() );
            if ( age < std::chrono::hours(24) )
              return true;

            MIL << "Removing GeoIP file for " << entry.name << " since it's older than 24hrs." << std::endl;
            zypp::filesystem::unlink( dir/entry.name );
            return true;
          });

          auto firstOfCb = [this]( std::string hostname ) {

            // do not query files that are still there
            if ( zypp::PathInfo( _geoIPCache / hostname ).isExist() )  {
              MIL << "Skipping GeoIP request for " << hostname << " since a valid cache entry exists." << std::endl;
              return makeReadyResult(false);
            }

            MIL << "Query GeoIP for " << hostname << std::endl;

            zypp::Url url;
            try {
              url.setHost(hostname);
              url.setScheme("https");

            } catch(const zypp::Exception &e ) {
              ZYPP_CAUGHT(e);
              MIL << "Ignoring invalid GeoIP hostname: " << hostname << std::endl;
              return makeReadyResult(false);
            }

            // always https ,but attaching makes things easier
            return _zyppCtx->provider()->attachMedia( url, ProvideMediaSpec() )
            | and_then( [this]( MediaHandle provideHdl ) { return _zyppCtx->provider()->provide( provideHdl, "/geoip", ProvideFileSpec() ); })
            | inspect_err( [hostname]( const std::exception_ptr& ){ MIL << "Failed to query GeoIP from hostname: " << hostname << std::endl; } )
            | and_then( [hostname, this]( ProvideRes provideRes ) {

              // here we got something from the server, we will stop after this hostname and mark the process as success()

              constexpr auto writeHostToFile = []( const zypp::Pathname &fName, const std::string &host ){
                std::ofstream out;
                out.open( fName.asString(), std::ios_base::trunc );
                if ( out.is_open() ) {
                  out << host << std::endl;
                } else {
                  MIL << "Failed to create/open GeoIP cache file " << fName << std::endl;
                }
              };

              std::string geoipMirror;
              try {
                zypp::xml::Reader reader( provideRes.file() );
                if ( reader.seekToNode( 1, "host" ) ) {
                  const auto &str = reader.nodeText().asString();

                  // make a dummy URL to ensure the hostname is valid
                  zypp::Url testUrl;
                  testUrl.setHost(str);
                  testUrl.setScheme("https");

                  if ( testUrl.isValid() ) {
                    MIL << "Storing geoIP redirection: " << hostname << " -> " << str << std::endl;
                    geoipMirror = str;
                  }

                } else {
                  MIL << "No host entry or empty file returned for GeoIP, remembering for 24hrs" << std::endl;
                }
              } catch ( const zypp::Exception &e ) {
                ZYPP_CAUGHT(e);
                MIL << "Empty or invalid GeoIP file, not requesting again for 24hrs" << std::endl;
              }

              writeHostToFile( _geoIPCache / hostname, geoipMirror );
              return expected<void>::success(); // need to return a expected<> due to and_then requirements
            })
            | []( expected<void> res ) { return res.is_valid(); };
          };

          return std::move(hosts)
          | firstOf( std::move(firstOfCb), false, zyppng::detail::ContinueUntilValidPredicate() )
          | []( bool foundGeoIP ) {

            if ( foundGeoIP ) {
              MIL << "Successfully queried GeoIP data." << std::endl;
              return expected<void>::success ();
            }

            MIL << "Failed to query GeoIP data." << std::endl;
            return expected<void>::error( std::make_exception_ptr( zypp::Exception("No valid geoIP url found" )) );

          };
        }

    private:
        ContextRef _zyppCtx;
        zypp::MirroredOriginSet _origins;
        zypp::Pathname     _geoIPCache;

    };
  }

  MaybeAwaitable<expected<void> > refreshGeoIPData( ContextRef ctx, RepoInfo::url_set urls )
  {
    if constexpr ( ZYPP_IS_ASYNC )
      return SimpleExecutor<RefreshGeoIpLogic, AsyncOp<expected<void>>>::run( std::move(ctx), zypp::MirroredOriginSet(urls) );
    else
      return SimpleExecutor<RefreshGeoIpLogic, SyncOp<expected<void>>>::run( std::move(ctx), zypp::MirroredOriginSet(urls) );
  }

  MaybeAwaitable<expected<void> > refreshGeoIPData(ContextRef ctx, zypp::MirroredOriginSet origins)
  {
    if constexpr ( ZYPP_IS_ASYNC )
      return SimpleExecutor<RefreshGeoIpLogic, AsyncOp<expected<void>>>::run( std::move(ctx), std::move(origins) );
    else
      return SimpleExecutor<RefreshGeoIpLogic, SyncOp<expected<void>>>::run( std::move(ctx), std::move(origins) );
  }

}
