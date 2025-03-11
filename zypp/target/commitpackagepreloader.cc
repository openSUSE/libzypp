#include "private/commitpackagepreloader_p.h"
#include "zypp-core/base/Gettext.h"
#include <zypp-curl/private/curlhelper_p.h>
#include <zypp-media/auth/credentialmanager.h>
#include <zypp/media/MediaCurl2.h> // for shared logic like authenticate
#include <zypp/media/MediaHandlerFactory.h> // to detect the URL type
#include <zypp-media/mediaconfig.h>
#include <zypp/media/UrlResolverPlugin.h>
#include <zypp-core/zyppng/base/eventloop.h>
#include <zypp-core/fs/TmpPath.h>
#include <zypp-curl/transfersettings.h>
#include <zypp-curl/ng/network/networkrequestdispatcher.h>
#include <zypp-curl/ng/network/request.h>
#include <zypp/MediaSetAccess.h>
#include <zypp/Package.h>
#include <zypp/SrcPackage.h>
#include <zypp/ZConfig.h>

namespace zypp {

  namespace {

    inline bool preloadEnabled()
    {
      const char *val = ::getenv("ZYPP_PCK_PRELOAD");

      // opt out for now
      if ( !val )
        return false;

      return ( std::string_view( val ) == "1" );
    }

    zypp::Pathname pckCachedLocation ( const PoolItem &pck ) {
      if ( pck.isKind<Package>() ) {
        return pck->asKind<Package>()->cachedLocation();
      } else if ( pck.isKind<SrcPackage>() ) {
        return pck->asKind<SrcPackage>()->cachedLocation();
      }
      return {};
    }

  }

  struct RepoUrl {
    zypp::Url baseUrl;
    media::UrlResolverPlugin::HeaderList headers;
  };

  class CommitPackagePreloader::PreloadWorker : public zyppng::Base {
  public:
    enum State {
      Pending,
      SimpleDl,
      //ZckHead,
      //ZckData,
      Finished
    };

    PreloadWorker( CommitPackagePreloader &parent ) : _parent(parent) {}

    bool finished ( ) const {
      return (_s == Finished);
    }

    void nextJob () {

      // clean state vars
      _started = false;
      _firstAuth = true;
      _notFoundRetry = 0;
      _tmpFile.reset();
      _lastByteCount = 0;
      _taintedMirrors.clear();

      if ( _parent._requiredDls.empty() ) {

        if ( _myMirror ) {
          _myMirror->refs--;
          _myMirror = nullptr;
          _currentRepoId = sat::detail::noRepoId;
        }

        MIL << "No more jobs pending, exiting worker" << std::endl;
        // exit!
        _s = Finished;
        _sigFinished.emit();
        return;
      }

      _job = _parent._requiredDls.front();
      _parent._requiredDls.pop_front();

      auto loc = _job.lookupLocation();
      _targetPath = _job.repoInfo().predownloadPath() / _job.lookupLocation().filename();

      // select a mirror we want to use
      if ( !prepareMirror( ) ) {
        callback::UserData userData( "CommitPreloadReport/fileDone" );
        userData.set( "description", _("No mirror found") );
        _parent._report->fileDone( _targetPath, media::CommitPreloadReport::ERROR, userData );
        return nextJob();
      }

      if ( filesystem::assert_dir( _targetPath.dirname()) != 0 ) {
        ERR << "Failed to create target dir for file: " << _targetPath << std::endl;
        callback::UserData userData( "CommitPreloadReport/fileDone" );
        userData.set( "description", _("Could not create target File") );
        _parent._report->fileDone( _targetPath, media::CommitPreloadReport::ERROR, userData );
        return nextJob();
      }


      media::TransferSettings settings;
      zypp::Url url;
      makeJobUrl ( url, settings );

      // check if the file is there already
      {
        PathInfo pathInfo(_targetPath);
        if ( pathInfo.isExist() ) {
          // just in case there is something else that is not a file we delete it
          if ( !pathInfo.isFile() ) {
            if ( pathInfo.isDir () )
              filesystem::rmdir( _targetPath );
            else
              filesystem::unlink( _targetPath );

          } else if ( is_checksum( _targetPath, loc.checksum() ) ) {

            // if we have the file already, no need to download again
            callback::UserData userData( "CommitPreloadReport/fileDone" );
            userData.set( "Url", url );
            userData.set( "description", _("Already in Cache") );
            _parent._report->fileDone( _targetPath, media::CommitPreloadReport::NO_ERROR, userData );

            return nextJob();

          } else {
            // everything else we delete
            filesystem::unlink ( _targetPath );
          }
        }
      }

      // we download into a temp file so that we don't leave broken files in case of errors or a crash
      _tmpFile = filesystem::TmpFile::asManagedFile( _targetPath.dirname(), _targetPath.basename() );

      if ( _s == Pending ) {
        // init case, set up request
        _req = std::make_shared<zyppng::NetworkRequest>( url, _tmpFile );
        _req->connect( &zyppng::NetworkRequest::sigStarted, *this, &PreloadWorker::onRequestStarted );
        _req->connect( &zyppng::NetworkRequest::sigBytesDownloaded, *this, &PreloadWorker::onRequestProgress );
        _req->connect( &zyppng::NetworkRequest::sigFinished, *this, &PreloadWorker::onRequestFinished );
      } else {
        _req->resetRequestRanges();
        _req->setUrl( url );
        _req->setTargetFilePath( _tmpFile );
      }

      // TODO check for zchunk

      _s = SimpleDl;
      _req->transferSettings() = settings;
      _parent._dispatcher->enqueue(_req);
    }

    zyppng::SignalProxy<void()> sigWorkerFinished() {
      return _sigFinished;
    }

  private:

    // TODO some smarter logic that selects mirrors
    bool prepareMirror( ) {

      const auto &pi = _job;

      if ( _myMirror ) {
        if ( _currentRepoId == pi.repository().id() ) {
          return true;
        }
        _currentRepoId = sat::detail::noRepoId;
        _myMirror->refs--;
        _myMirror = nullptr;
      }

      _myMirror = findUsableMirror ();
      if ( !_myMirror )
        return false;

      _currentRepoId = pi.repository().id();
      _myMirror->refs++;
      return true;
    }

    /**
   * Taints the current mirror, returns true if a alternative was found
   */
    bool taintCurrentMirror() {

      if ( _myMirror ) {
        _myMirror->miss++;
        _taintedMirrors.insert( _myMirror );
      }

      // try to find another mirror
      auto mirrPtr = findUsableMirror ( _myMirror, false );
      if ( mirrPtr ) {
        if ( _myMirror ) {
          _myMirror->refs--;
        }
        _myMirror = mirrPtr;
        _myMirror->refs++;
        return true;
      }
      return false;
    }

    /**
   * Tries to find a usable mirror
   */
    RepoUrl *findUsableMirror( RepoUrl *skip = nullptr, bool allowTainted = true ) {
      auto &repoDlInfo = _parent._dlRepoInfo.at( _job.repository().id() );

      std::vector<RepoUrl>::iterator curr = repoDlInfo._baseUrls.end();
      int currentSmallestRef = INT_MAX;

      for ( auto i = repoDlInfo._baseUrls.begin(); i != repoDlInfo._baseUrls.end(); i++ ) {
        auto mirrorPtr = &(*i);

        if ( skip == mirrorPtr )
          continue;

        if ( !allowTainted && _taintedMirrors.find(mirrorPtr) != _taintedMirrors.end() )
          continue;

        // we are adding the file misses on top of the refcount
        // that way we will use mirrors that often miss a file less
        if ( ( i->refs + i->miss ) < currentSmallestRef ) {
          currentSmallestRef = ( i->refs + i->miss );
          curr = i;
        }
      }

      if ( curr == repoDlInfo._baseUrls.end() )
        return nullptr;
      return &(*curr);
    }

    void onRequestStarted ( zyppng::NetworkRequest &req ) {
      MIL << "Request for " << req.url() << " started" << std::endl;
    }

    void onRequestProgress( zyppng::NetworkRequest &req, zypp::ByteCount count ) {
      if ( !_started ) {
        _started = true;

        callback::UserData userData( "CommitPreloadReport/fileStart" );
        userData.set( "Url", _req->url() );
        _parent._report->fileStart( _targetPath, userData );
      }

      ByteCount downloaded;
      if ( _lastByteCount == 0 )
        downloaded = count;
      else
        downloaded = count - _lastByteCount;
      _lastByteCount = count;

      _parent.reportBytesDownloaded( downloaded );
    }

    void onRequestFinished( zyppng::NetworkRequest &req, const zyppng::NetworkRequestError &err ) {
      MIL << "Request for " << req.url() << " finished. (" << err.toString() << ")" << std::endl;
      if ( !req.hasError() ) {
        if ( filesystem::rename( _tmpFile, _targetPath ) != 0 ) {
          // error
          failCurrentJob ( _targetPath, req.url(), media::CommitPreloadReport::ERROR, _("Failed to rename temporary file.") );
        } else {
          callback::UserData userData( "CommitPreloadReport/fileDone" );
          userData.set( "Url", req.url() );
          userData.set( "description", _("Finished") );
          _parent._report->fileDone( _targetPath, media::CommitPreloadReport::NO_ERROR, userData );
        }
      } else {
        // handle errors and auth
        const auto &error = req.error();
        switch ( error.type() ) {
          case zyppng::NetworkRequestError::InternalError:
          case zyppng::NetworkRequestError::InvalidChecksum:
          case zyppng::NetworkRequestError::UnsupportedProtocol:
          case zyppng::NetworkRequestError::MalformedURL:
          case zyppng::NetworkRequestError::PeerCertificateInvalid:
          case zyppng::NetworkRequestError::ConnectionFailed:
          case zyppng::NetworkRequestError::ServerReturnedError:
          case zyppng::NetworkRequestError::MissingData:
          case zyppng::NetworkRequestError::RangeFail:
          case zyppng::NetworkRequestError::Cancelled:
          case zyppng::NetworkRequestError::ExceededMaxLen:
          case zyppng::NetworkRequestError::TemporaryProblem:
          case zyppng::NetworkRequestError::Timeout:
          case zyppng::NetworkRequestError::Forbidden:
          case zyppng::NetworkRequestError::Http2Error:
          case zyppng::NetworkRequestError::Http2StreamError: {
            failCurrentJob ( _targetPath, req.url(), media::CommitPreloadReport::ERROR, req.extendedErrorString() );
            break;
          }
          case zyppng::NetworkRequestError::Unauthorized:
          case zyppng::NetworkRequestError::AuthFailed: {

            //in case we got a auth hint from the server the error object will contain it
            std::string authHint = error.extraInfoValue("authHint", std::string());

            media::CredentialManager cm(media::CredManagerOptions(ZConfig::instance().repoManagerRoot()));
            bool newCreds = media::MediaCurl2::authenticate( _myMirror->baseUrl, cm, req.transferSettings(), authHint, _firstAuth );
            if ( newCreds) {
              _firstAuth = false;
              _parent._dispatcher->enqueue( _req );
              return;
            }

            failCurrentJob ( _targetPath, req.url(), media::CommitPreloadReport::ACCESS_DENIED, req.extendedErrorString() );
            break;
          }
          case zyppng::NetworkRequestError::NotFound: {

            MIL << "Download from mirror failed for file " << req.url () << " trying to taint mirror and move on" << std::endl;

            if ( taintCurrentMirror() ) {
              _notFoundRetry++;

              media::TransferSettings settings;
              zypp::Url url;
              makeJobUrl ( url, settings );

              MIL << "Found new mirror: " << url << " recovering, retry count: " << _notFoundRetry << std::endl;

              _req->setUrl( url );
              _req->transferSettings () = settings;

              _parent._dispatcher->enqueue( _req );
              return;
            }

            failCurrentJob ( _targetPath, req.url(), media::CommitPreloadReport::NOT_FOUND, req.extendedErrorString() );
            break;
          }
          case zyppng::NetworkRequestError::NoError:
            // should never happen
            DBG << "BUG: Download error flag is set , but Error code is NoError" << std::endl;
            break;
        }
      }
      nextJob();
    }

    void failCurrentJob( const zypp::Pathname &localPath, const std::optional<zypp::Url> &url, media::CommitPreloadReport::Error e, const std::optional<std::string> &errorMessage ) {

      callback::UserData userData( "CommitPreloadReport/fileDone" );
      if ( url )
        userData.set( "Url", url );
      if ( errorMessage )
        userData.set( "description", _("Already in Cache") );

      _parent._missedDownloads = true;
      _parent._report->fileDone( localPath, e, userData );
    }

    void makeJobUrl ( zypp::Url &resultUrl, media::TransferSettings &resultSet ) {

      // rewrite Url
      zypp::Url url = _myMirror->baseUrl;
      media::TransferSettings settings;

      // TODO share logic with MediaCurl2
      ::internal::fillSettingsFromUrl( url, settings );

      // if the proxy was not set (or explicitly unset) by url, then look...
      if ( settings.proxy().empty() )
        ::internal::fillSettingsSystemProxy( url, settings );

      // remove extra options from the URL
      url = ::internal::clearQueryString( url );

      const auto &loc = _job.lookupLocation();

      // rewrite URL for media handle
      url = MediaSetAccess::rewriteUrl( url ,loc.medianr() );

      // append path to file
      url.appendPathName( loc.filename() );

      // add extra headers
      for ( const auto & el : _myMirror->headers ) {
        std::string header { el.first };
        header += ": ";
        header += el.second;
        MIL << "Added custom header -> " << header << std::endl;
        settings.addHeader( std::move(header) );
      }

      resultUrl = url;
      resultSet = settings;
    }

  private:
    State _s = Pending;
    CommitPackagePreloader &_parent;
    zyppng::NetworkRequestRef _req;

    PoolItem    _job;
    ManagedFile _tmpFile;
    zypp::Pathname _targetPath;
    bool _started = false;
    bool _firstAuth = true;
    RepoUrl *_myMirror = nullptr;
    ByteCount _lastByteCount = 0;
    Repository::IdType _currentRepoId = sat::detail::noRepoId;

    // retry handling
    int _notFoundRetry = 0;
    std::set<RepoUrl *> _taintedMirrors; //< mirrors that returned 404 for the current request

    zyppng::Signal<void()> _sigFinished;

  };

  CommitPackagePreloader::CommitPackagePreloader()
  {}

  void CommitPackagePreloader::preloadTransaction( const std::vector<sat::Transaction::Step> &steps)
  {
    if ( !preloadEnabled() ) {
      MIL << "CommitPackagePreloader disabled" << std::endl;
      return;
    }

    // preload happens only if someone handles the report
    if ( !_report->connected() ) {
      MIL << "No receiver for the CommitPreloadReport, skipping preload phase" << std::endl;
      return;
    }

    auto ev = zyppng::EventLoop::create();
    _dispatcher = std::make_shared<zyppng::NetworkRequestDispatcher>();
    _dispatcher->setMaximumConcurrentConnections( MediaConfig::instance().download_max_concurrent_connections() );
    _dispatcher->setAgentString ( str::asString( media::MediaCurl2::agentString () ) );
    _dispatcher->setHostSpecificHeader ("download.opensuse.org", "X-ZYpp-DistributionFlavor", str::asString(media::MediaCurl2::distributionFlavorHeader()) );
    _dispatcher->setHostSpecificHeader ("download.opensuse.org", "X-ZYpp-AnonymousId", str::asString(media::MediaCurl2::anonymousIdHeader()) );
    _dispatcher->run();

    _pTracker = std::make_shared<internal::ProgressTracker>();
    _requiredBytes   = 0;
    _downloadedBytes = 0;
    _missedDownloads = false;

    zypp_defer {
      _dispatcher.reset();
      _pTracker.reset();
    };

    for ( const auto &step : steps ) {
      switch ( step.stepType() )
      {
        case sat::Transaction::TRANSACTION_INSTALL:
        case sat::Transaction::TRANSACTION_MULTIINSTALL:
          // proceed: only install actions may require download.
          break;

        default:
          // next: no download for non-packages and delete actions.
          continue;
          break;
      }

      PoolItem pi(step.satSolvable());

      if ( !pi->isKind<Package>() && !pi->isKind<SrcPackage>() )
        continue;

      // no checksum ,no predownload, Fetcher would ignore it
      if ( pi->lookupLocation().checksum().empty() )
        continue;

      // check if Package is cached already
      if( !pckCachedLocation(pi).empty() )
        continue;

      auto repoDlsIter = _dlRepoInfo.find( pi.repository().id() );
      if ( repoDlsIter == _dlRepoInfo.end() ) {

        // make sure download path for this repo exists
        if ( filesystem::assert_dir( pi.repoInfo().predownloadPath()  ) != 0 ) {
          ERR << "Failed to create predownload cache for repo " << pi.repoInfo().alias() << std::endl;
          return;
        }

        // filter base URLs that do not download
        std::vector<RepoUrl> repoUrls;
        const auto bu = pi.repoInfo().baseUrls();
        std::for_each( bu.begin(), bu.end(), [&]( const zypp::Url &u ) {
          media::UrlResolverPlugin::HeaderList custom_headers;
          Url url = media::UrlResolverPlugin::resolveUrl(u, custom_headers);
          MIL << "Trying scheme '" << url.getScheme() << "'" << std::endl;

          if ( media::MediaHandlerFactory::handlerType(url) != media::MediaHandlerFactory::MediaCURLType )
            return;

          // use geo IP if available
          {
            const auto rewriteUrl = media::MediaNetworkCommonHandler::findGeoIPRedirect( url );
            if ( rewriteUrl.isValid () )
              url = rewriteUrl;
          }

          repoUrls.push_back( RepoUrl {
                                .baseUrl = std::move(url),
                                .headers = std::move(custom_headers)
                              } );
        });

        // skip this solvable if it has no downloading base URLs
        if( repoUrls.empty() ) {
          MIL << "Skipping predownload for " << step.satSolvable() << " no downloading URL" << std::endl;
          continue;
        }

        // TODO here we could block to fetch mirror informations, either if the RepoInfo has a metalink or mirrorlist entry
        // or if the hostname of the repo is d.o.o
        if ( repoUrls.begin()->baseUrl.getHost() == "download.opensuse.org" ){
          //auto req = std::make_shared<zyppng::NetworkRequest>(  );
        }

        _dlRepoInfo.insert( std::make_pair(
                              pi.repository().id(),
                              RepoDownloadData{
                                ._baseUrls = std::move(repoUrls)
                              }
                              ));
      }


      _requiredBytes += pi.lookupLocation().downloadSize();
      _requiredDls.push_back( pi );
    }

    if ( _requiredDls.empty() )
      return;

    // order by repo
    std::sort( _requiredDls.begin(), _requiredDls.end(), []( const PoolItem &a , const PoolItem &b ) { return a.repository() < b.repository(); });

    const auto &workerDone = [&, this](){
      if ( std::all_of( _workers.begin(), _workers.end(), []( const auto &w ) { return w->finished();} ) )
        ev->quit();
    };

    _report->start();
    zypp_defer {
      _report->finish( _missedDownloads ? media::CommitPreloadReport::MISS : media::CommitPreloadReport::SUCCESS );
    };

    MIL << "Downloading packages via " << MediaConfig::instance().download_max_concurrent_connections() << " connections." << std::endl;

    // we start a worker for each configured connection
    for ( int i = 0; i < MediaConfig::instance().download_max_concurrent_connections() ; i++ ) {
      // if we run out of jobs before we started all workers, stop
      if (_requiredDls.empty())
        break;
      auto worker = std::make_shared<PreloadWorker>(*this);
      worker->sigWorkerFinished().connect(workerDone);
      worker->nextJob();
      _workers.push_back( std::move(worker) );
    }

    if( std::any_of( _workers.begin(), _workers.end(), []( const auto &w ) { return !w->finished(); } ) ) {
      MIL << "Running preload event loop!" << std::endl;
      ev->run();
    }

    MIL << "Preloading done, returning" << std::endl;
  }

  void CommitPackagePreloader::cleanupCaches()
  {
    if ( !preloadEnabled() ) {
      MIL << "CommitPackagePreloader disabled" << std::endl;
      return;
    }
    std::for_each( _dlRepoInfo.begin (), _dlRepoInfo.end(), []( const auto &elem ){
      filesystem::clean_dir ( Repository(elem.first).info().predownloadPath() );
    });
  }

  bool CommitPackagePreloader::missed() const
  {
    return _missedDownloads;
  }

  void CommitPackagePreloader::reportBytesDownloaded(ByteCount newBytes)
  {
    _downloadedBytes += newBytes;
    _pTracker->updateStats( _requiredBytes, _downloadedBytes );

    callback::UserData userData( "CommitPreloadReport/progress" );
    userData.set( "dbps_avg"    ,   static_cast<double>( _pTracker->_drateTotal ) );
    userData.set( "dbps_current",   static_cast<double>( _pTracker->_drateLast ) );
    userData.set( "bytesReceived",  static_cast<double>( _pTracker->_dnlNow ) );
    userData.set( "bytesRequired",  static_cast<double>( _pTracker->_dnlTotal ) );
    if ( !_report->progress( _pTracker->_dnlPercent, userData ) ) {
      _missedDownloads = true;
      _requiredDls.clear();
      _dispatcher->cancelAll( _("Cancelled by user."));
    }
  }

}
