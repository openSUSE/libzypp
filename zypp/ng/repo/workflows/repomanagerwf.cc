/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "repomanagerwf.h"

#include <zypp-core/ManagedFile.h>
#include <utility>
#include <zypp-core/zyppng/pipelines/MTry>
#include <zypp-media/MediaException>
#include <zypp-media/ng/Provide>
#include <zypp-media/ng/ProvideSpec>

#include <zypp/ng/Context>
#include <zypp/ng/workflows/logichelpers.h>
#include <zypp/ng/workflows/contextfacade.h>
#include <zypp/ng/repo/workflows/repodownloaderwf.h>

namespace zyppng {

  using namespace zyppng::operators;

  namespace {

  template <class Executor, class OpType>
  struct ProbeRepoLogic : public LogicBase<Executor, OpType>
  {
  protected:
    ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

  public:
    using ZyppContextRefType = std::conditional_t<zyppng::detail::is_async_op_v<OpType>, ContextRef, SyncContextRef >;
    using ProvideType     = typename remove_smart_ptr_t<ZyppContextRefType>::ProvideType;
    using MediaHandle     = typename ProvideType::MediaHandle;
    using ProvideRes      = typename ProvideType::Res;

    ProbeRepoLogic(ZyppContextRefType zyppCtx, MediaHandle &&medium, zypp::Pathname &&path, std::optional<zypp::Pathname> &&targetPath )
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

      std::shared_ptr<ProvideType> providerRef = _zyppContext->provider();

      // TranslatorExplanation '%s' is an URL
      _error = zypp::repo::RepoException (zypp::str::form( _("Error trying to read from '%s'"), url.asString().c_str() ));

      // first try rpmmd
      return providerRef->provide( _medium, _path/"repodata/repomd.xml", ProvideFileSpec().setCheckExistsOnly( !_targetPath.has_value() ) )
        | and_then( maybeCopyResultToDest("repodata/repomd.xml") )
        | and_then( [](){ return expected<zypp::repo::RepoType>::success(zypp::repo::RepoType::RPMMD); } )
        // try susetags if rpmmd fails and remember the error
        | or_else( [this, providerRef]( std::exception_ptr &&err ) {
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
            return makeReadyResult( expected<zypp::repo::RepoType>::error( std::current_exception() ) );
          }
          return providerRef->provide( _medium, _path/"content", ProvideFileSpec().setCheckExistsOnly( !_targetPath.has_value() ) )
              | and_then( maybeCopyResultToDest("content") )
              | and_then( []()->expected<zypp::repo::RepoType>{ return expected<zypp::repo::RepoType>::success(zypp::repo::RepoType::YAST2); } );
        })
        // no rpmmd and no susetags!
        | or_else( [this]( std::exception_ptr &&err ) {

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
            return expected<zypp::repo::RepoType>::error( ZYPP_EXCPT_PTR(e) );
          } catch(...) {
            // any other error, we give up
            return expected<zypp::repo::RepoType>::error( std::current_exception() );
          }

          const auto &url = _medium.baseUrl();

          // if it is a non-downloading URL denoting a directory (bsc#1191286: and no plugin)
          if ( ! ( url.schemeIsDownloading() || url.schemeIsPlugin() ) ) {

            if ( _medium.localPath() && zypp::PathInfo(_medium.localPath().value()/_path).isDir() ) {
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
              | ProvideType::copyResultToDest( _zyppContext->provider(), *_targetPath/subPath)
              | and_then([this]( zypp::ManagedFile file ){ file.resetDispose(); return expected<void>::success(); } );
        }
        return makeReadyResult( expected<void>::success() );
      };
    }

  private:
    ZyppContextRefType _zyppContext;
    MediaHandle _medium;
    zypp::Pathname _path;
    std::optional<zypp::Pathname> _targetPath;

    zypp::repo::RepoException _error;
    bool _gotMediaError = false;
  };

  }

  AsyncOpRef<expected<zypp::repo::RepoType> > RepoManagerWorkflow::probeRepoType(ContextRef ctx, ProvideMediaHandle medium, zypp::Pathname path, std::optional<zypp::Pathname> targetPath)
  {
    return SimpleExecutor< ProbeRepoLogic, AsyncOp<expected<zypp::repo::RepoType>> >::run( std::move(ctx), std::move(medium), std::move(path), std::move(targetPath) );
  }

  expected<zypp::repo::RepoType> RepoManagerWorkflow::probeRepoType(SyncContextRef ctx, SyncMediaHandle medium, zypp::Pathname path, std::optional<zypp::Pathname> targetPath )
  {
    return SimpleExecutor< ProbeRepoLogic, SyncOp<expected<zypp::repo::RepoType>> >::run( std::move(ctx), std::move(medium), std::move(path), std::move(targetPath) );
  }



  namespace {

    template<typename Executor, class OpType>
    struct CheckIfToRefreshMetadataLogic : public LogicBase<Executor, OpType> {

      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);
    public:

      using RefreshContextRefType = std::conditional_t<zyppng::detail::is_async_op_v<OpType>, repo::AsyncRefreshContextRef, repo::SyncRefreshContextRef>;
      using ZyppContextRefType = typename RefreshContextRefType::element_type::ContextRefType;
      using ZyppContextType = typename RefreshContextRefType::element_type::ContextType;
      using ProvideType     = typename ZyppContextType::ProvideType;
      using MediaHandle     = typename ProvideType::MediaHandle;
      using ProvideRes      = typename ProvideType::Res;

      CheckIfToRefreshMetadataLogic( RefreshContextRefType refCtx, MediaHandle &&medium, ProgressObserverRef progressObserver )
        : _refreshContext(std::move(refCtx))
        , _progress(std::move( progressObserver ))
        , _medium(std::move( medium ))
      {}

      MaybeAsyncRef<expected<repo::RefreshCheckStatus>> execute( ) {

        MIL << "Going to CheckIfToRefreshMetadata" << std::endl;

        return mtry( static_cast<void(*)(const zypp::RepoInfo &)>(zypp::assert_alias), _refreshContext->repoInfo() )
        // | and_then( refreshGeoIPData( { url } ); )  << should this be even here? probably should be put where we attach the medium
        | and_then( [this](){

          const auto &info = _refreshContext->repoInfo();

          MIL << "Check if to refresh repo " << _refreshContext->repoInfo().alias() << " at " << _medium.baseUrl() << " (" << info.type() << ")" << std::endl;

          _mediarootpath = _refreshContext->rawCachePath();
          zypp::filesystem::assert_dir(_mediarootpath );

          // first check old (cached) metadata
          zypp::RepoStatus oldstatus = zypp::RepoManagerBaseImpl::metadataStatus( info, _refreshContext->repoManagerOptions() );

          if ( oldstatus.empty() ) {
            MIL << "No cached metadata, going to refresh" << std::endl;
            return makeReadyResult( expected<repo::RefreshCheckStatus>::success(repo::REFRESH_NEEDED) );
          }

          if ( _medium.baseUrl().schemeIsVolatile() ) {
            MIL << "Never refresh CD/DVD" << std::endl;
            return makeReadyResult( expected<repo::RefreshCheckStatus>::success(repo::REPO_UP_TO_DATE) );
          }

          if ( _refreshContext->policy() == repo::RefreshForced ) {
            MIL << "Forced refresh!" << std::endl;
            return makeReadyResult( expected<repo::RefreshCheckStatus>::success(repo::REFRESH_NEEDED) );
          }

          if ( _medium.baseUrl().schemeIsLocal() ) {
            _refreshContext->setPolicy( repo::RefreshIfNeededIgnoreDelay );
          }

          // Check whether repo.refresh.delay applies...
          if ( _refreshContext->policy() != repo::RefreshIfNeededIgnoreDelay )
          {
            // bsc#1174016: Prerequisite to skipping the refresh is that metadata
            // and solv cache status match. They will not, if the repos URL was
            // changed e.g. due to changed repovars.
            zypp::RepoStatus cachestatus = zypp::RepoManagerBaseImpl::cacheStatus( info, _refreshContext->repoManagerOptions() );

            if ( oldstatus == cachestatus ) {
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
                  return makeReadyResult( expected<repo::RefreshCheckStatus>::success(repo::REPO_CHECK_DELAYED) );
                }
              }
            }
            else {
              MIL << "Metadata and solv cache don't match. Check data on server..." << std::endl;
            }
          }

          return info.type() | [this]( zypp::repo::RepoType &&repokind ) {
            // if unknown: probe it
            if ( repokind == zypp::repo::RepoType::NONE )
              return RepoManagerWorkflow::probeRepoType( _refreshContext->zyppContext(), _medium, _refreshContext->repoInfo().path(), _refreshContext->targetDir() );
            return makeReadyResult( expected<zypp::repo::RepoType>::success(repokind) );
          } | and_then([this, oldstatus]( zypp::repo::RepoType &&repokind ) {

            // make sure to remember the repo type
            _refreshContext->repoInfo().setProbedType( repokind );

            auto dlContext = std::make_shared<repo::DownloadContext<ZyppContextRefType>>( _refreshContext->zyppContext(), _refreshContext->repoInfo(), _refreshContext->targetDir() );
            return RepoDownloaderWorkflow::repoStatus ( dlContext, _medium )
              | and_then( [this, dlContext, oldstatus]( zypp::RepoStatus &&newstatus ){
                // check status
                if ( oldstatus == newstatus ) {
                  MIL << "repo has not changed" << std::endl;
                  zypp::RepoManagerBaseImpl::touchIndexFile( _refreshContext->repoInfo(), _refreshContext->repoManagerOptions() );
                  return expected<repo::RefreshCheckStatus>::success(repo::REPO_UP_TO_DATE);
                }
                else { // includes newstatus.empty() if e.g. repo format changed
                  MIL << "repo has changed, going to refresh" << std::endl;
                  MIL << "Old status: " << oldstatus << " New Status: " << newstatus << std::endl;
                  return expected<repo::RefreshCheckStatus>::success(repo::REFRESH_NEEDED);
                }
              });
          });
        });
      }

    protected:
      RefreshContextRefType _refreshContext;
      ProgressObserverRef _progress;
      MediaHandle _medium;
      zypp::Pathname _mediarootpath;
    };
  }

  AsyncOpRef<expected<repo::RefreshCheckStatus> > RepoManagerWorkflow::checkIfToRefreshMetadata(repo::AsyncRefreshContextRef refCtx, ProvideMediaHandle medium, ProgressObserverRef progressObserver)
  {
    return SimpleExecutor< CheckIfToRefreshMetadataLogic , AsyncOp<expected<repo::RefreshCheckStatus>> >::run( std::move(refCtx), std::move(medium), std::move(progressObserver) );
  }

  expected<repo::RefreshCheckStatus> RepoManagerWorkflow::checkIfToRefreshMetadata(repo::SyncRefreshContextRef refCtx, SyncMediaHandle medium, ProgressObserverRef progressObserver)
  {
    return SimpleExecutor< CheckIfToRefreshMetadataLogic , SyncOp<expected<repo::RefreshCheckStatus>> >::run( std::move(refCtx), std::move(medium), std::move(progressObserver) );
  }


  namespace {

    template<typename Executor, class OpType>
    struct RefreshMetadataLogic : public LogicBase<Executor, OpType>{

      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

    public:

      using RefreshContextRefType = std::conditional_t<zyppng::detail::is_async_op_v<OpType>, repo::AsyncRefreshContextRef, repo::SyncRefreshContextRef>;
      using ZyppContextRefType = typename RefreshContextRefType::element_type::ContextRefType;
      using ZyppContextType    = typename RefreshContextRefType::element_type::ContextType;
      using ProvideType        = typename ZyppContextType::ProvideType;
      using MediaHandle        = typename ProvideType::MediaHandle;
      using ProvideRes         = typename ProvideType::Res;

      using DlContextType    = repo::DownloadContext<ZyppContextRefType>;
      using DlContextRefType = std::shared_ptr<DlContextType>;

      RefreshMetadataLogic( RefreshContextRefType refCtx, MediaHandle &&medium, ProgressObserverRef progressObserver )
        : _refreshContext(std::move(refCtx))
        , _progress ( std::move( progressObserver ) )
        , _medium   ( std::move( medium ) )
      {
        MIL << "Constructor called" << std::endl;
      }

      MaybeAsyncRef<expected<RefreshContextRefType>> execute() {

        // manually resolv the overloaded func
        constexpr auto assert_alias_cb = static_cast<void (*)( const zypp::RepoInfo &)>(zypp::assert_alias);

        return mtry(assert_alias_cb, _refreshContext->repoInfo() )
        | and_then( [this](){ return mtry(zypp::assert_urls, _refreshContext->repoInfo() ); })
        | and_then( [this](){ return RepoManagerWorkflow::checkIfToRefreshMetadata ( _refreshContext, _medium, _progress ); })
        | and_then( [this]( repo::RefreshCheckStatus &&status ){

          MIL << "RefreshCheckStatus returned: " << status << std::endl;

          // check whether to refresh metadata
          // if the check fails for this url, it throws, so another url will be checked
          if ( status != repo::REFRESH_NEEDED )
            return makeReadyResult ( expected<RefreshContextRefType>::success( std::move(_refreshContext) ) );

          MIL << "Going to refresh metadata from " << _medium.baseUrl() << std::endl;

          // bsc#1048315: Always re-probe in case of repo format change.
          // TODO: Would be sufficient to verify the type and re-probe
          // if verification failed (or type is RepoType::NONE)
          return RepoManagerWorkflow::probeRepoType ( _refreshContext->zyppContext(), _medium, _refreshContext->repoInfo().path(), _refreshContext->targetDir() )
          | and_then([this]( zypp::repo::RepoType &&repokind ) {

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
          | and_then([this]( DlContextRefType &&downloadContext  ) {

            // ok we have the metadata, now exchange
            // the contents
            _refreshContext->saveToRawCache();
            // if ( ! isTmpRepo( info ) )
            //  reposManip();	// remember to trigger appdata refresh

            // we are done.
            return expected<RefreshContextRefType>::success( std::move(_refreshContext) );
          })

          ;
        });




#if 0
        assert_alias(info);
        assert_urls(info);

        // make sure geoIP data is up 2 date
        refreshGeoIPData( info.baseUrls() );

        // we will throw this later if no URL checks out fine
        RepoException rexception( info, PL_("Valid metadata not found at specified URL",
                                            "Valid metadata not found at specified URLs",
                                            info.baseUrlsSize() ) );

        // Suppress (interactive) media::MediaChangeReport if we in have multiple basurls (>1)
        media::ScopedDisableMediaChangeReport guard( info.baseUrlsSize() > 1 );
        // try urls one by one
        for ( RepoInfo::urls_const_iterator it = info.baseUrlsBegin(); it != info.baseUrlsEnd(); ++it )
        {
          try
          {
            Url url(*it);

            // check whether to refresh metadata
            // if the check fails for this url, it throws, so another url will be checked
            if (checkIfToRefreshMetadata(info, url, policy)!=REFRESH_NEEDED)
              return;

            MIL << "Going to refresh metadata from " << url << endl;


            repo::RepoType repokind = info.type();
            {
              repo::RepoType probed = probe( *it, info.path() );


            }

            aaaa

            if ( ( repokind.toEnum() == RepoType::RPMMD_e ) ||
                 ( repokind.toEnum() == RepoType::YAST2_e ) )
            {
              MediaSetAccess media(url);
              shared_ptr<repo::Downloader> downloader_ptr;

              MIL << "Creating downloader for [ " << info.alias() << " ]" << endl;

              if ( repokind.toEnum() == RepoType::RPMMD_e ) {
                downloader_ptr.reset(new yum::Downloader(info, mediarootpath ));
                if ( _pluginRepoverification.checkIfNeeded() )
                  downloader_ptr->setPluginRepoverification( _pluginRepoverification ); // susetags is dead so we apply just to yum
              }
              else
                downloader_ptr.reset( new susetags::Downloader(info, mediarootpath) );

              /**
               * Given a downloader, sets the other repos raw metadata
               * path as cache paths for the fetcher, so if another
               * repo has the same file, it will not download it
               * but copy it from the other repository
               */
              for_( it, repoBegin(), repoEnd() )
              {
                Pathname cachepath(rawcache_path_for_repoinfo( _options, *it ));
                if ( PathInfo(cachepath).isExist() )
                  downloader_ptr->addCachePath(cachepath);

              }

              downloader_ptr->download( media, tmpdir.path() );
            }
            else if ( repokind.toEnum() == RepoType::RPMPLAINDIR_e )
            {
              // as substitute for real metadata remember the checksum of the directory we refreshed
              MediaMounter media( url );
              RepoStatus newstatus = RepoStatus( media.getPathName( info.path() ) );	// dir status

              Pathname productpath( tmpdir.path() / info.path() );
              filesystem::assert_dir( productpath );
              newstatus.saveToCookieFile( productpath/"cookie" );
            }
            else
            {
              ZYPP_THROW(RepoUnknownTypeException( info ));
            }

            // ok we have the metadata, now exchange
            // the contents
            filesystem::exchange( tmpdir.path(), mediarootpath );
            if ( ! isTmpRepo( info ) )
              reposManip();	// remember to trigger appdata refresh

            // we are done.
            return;
          }
          catch ( const Exception &e )
          {
            ZYPP_CAUGHT(e);
            ERR << "Trying another url..." << endl;

            // remember the exception caught for the *first URL*
            // if all other URLs fail, the rexception will be thrown with the
            // cause of the problem of the first URL remembered
            if (it == info.baseUrlsBegin())
              rexception.remember(e);
            else
              rexception.addHistory(  e.asUserString() );

          }
        } // for every url
        ERR << "No more urls..." << endl;
        ZYPP_THROW(rexception);

#endif


      }

      RefreshContextRefType _refreshContext;
      ProgressObserverRef _progress;
      MediaHandle _medium;
      zypp::Pathname _mediarootpath;

    };
  }

  namespace RepoManagerWorkflow {
    AsyncOpRef<expected<repo::AsyncRefreshContextRef> > refreshMetadata( repo::AsyncRefreshContextRef refCtx, ProvideMediaHandle medium, ProgressObserverRef progressObserver )
    {
      return SimpleExecutor<RefreshMetadataLogic, AsyncOp<expected<repo::AsyncRefreshContextRef>>>::run( std::move(refCtx), std::move(medium), std::move(progressObserver));
    }

    expected<repo::SyncRefreshContextRef> refreshMetadata( repo::SyncRefreshContextRef refCtx, SyncMediaHandle medium, ProgressObserverRef progressObserver )
    {
      return SimpleExecutor<RefreshMetadataLogic, SyncOp<expected<repo::SyncRefreshContextRef>>>::run( std::move(refCtx), std::move(medium), std::move(progressObserver));
    }
  }
}
