#include "private/commitpackagepreloader_p.h"
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

class CommitPackagePreloader::PreloadWorker {
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

  // TODO some smarter logic that selects mirrors
  // should work on hostname, not repo base URLs
  void prepareMirror( const PoolItem &pi ) {

    if ( _myMirror ) {
      if ( _currentRepoId == pi.repository().id() ) {
        return;
      }
      _myMirror->refs--;
    }

    _currentRepoId = pi.repository().id();

    auto &repoDlInfo = _parent._dlRepoInfo.at( pi.repository().id() );

    std::vector<RepoUrl>::iterator i = repoDlInfo._baseUrls.begin();
    std::vector<RepoUrl>::iterator curr = i;
    int currentSmallestRef = i->refs;
    i++;

    for ( ;i != repoDlInfo._baseUrls.end(); i++ ) {
      if ( i->refs < currentSmallestRef ) {
        currentSmallestRef = i->refs;
        curr = i;
      }
    }

    _myMirror = &(*curr);
    _myMirror->refs++;
  }

  void nextJob () {

    // clean state vars
    _started = false;
    _firstAuth = true;
    _notFoundRetry = 0;
    _tmpFile.reset();
    _lastByteCount = 0;

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

    // select a mirror we want to use
    prepareMirror( _job );

    auto loc = _job.lookupLocation();

    _targetPath = _job.repoInfo().predownloadPath() / _job.lookupLocation().filename();
    if ( filesystem::assert_dir( _targetPath.dirname()) != 0 ) {
      ERR << "Failed to create target dir for file: " << _targetPath << std::endl;
      return nextJob();
    }

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

    // rewrite URL for media handle
    url = MediaSetAccess::rewriteUrl( url ,loc.medianr() );

    // append path to file
    url.appendPathName( loc.filename() );

    // TODO check if file exists in download cache and if we can reuse it
    // -> no checksum avail -> IFMODSINCE

    // we download into a temp file so that we don't leave broken files in case of errors or a crash
    _tmpFile = filesystem::TmpFile::asManagedFile( _targetPath.dirname(), _targetPath.basename() );

    // add extra headers
    for ( const auto & el : _myMirror->headers ) {
      std::string header { el.first };
      header += ": ";
      header += el.second;
      MIL << "Added custom header -> " << header << std::endl;
      settings.addHeader( std::move(header) );
    }

    if ( _s == Pending ) {
      // init case, set up request
      _req = std::make_shared<zyppng::NetworkRequest>( url, _tmpFile );
      _req->sigStarted().connect( sigc::mem_fun( this, &PreloadWorker::onRequestStarted ) );
      _req->sigBytesDownloaded().connect( sigc::mem_fun( this, &PreloadWorker::onRequestProgress ) );
      _req->sigFinished().connect( sigc::mem_fun( this, &PreloadWorker::onRequestFinished ) );
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

  void onRequestStarted ( zyppng::NetworkRequest &req ) {
    MIL << "Request for " << req.url() << " started" << std::endl;
  }

  void onRequestProgress( zyppng::NetworkRequest &req, zypp::ByteCount count ) {
    if ( !_started ) {
      _started = true;
      _parent._report->fileStart( _req->url(), _targetPath );
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
        _parent._report->fileDone( req.url(), media::CommitPreloadReport::ERROR, "Things went wrong" );
      } else {
        _parent._report->fileDone( req.url(), media::CommitPreloadReport::NO_ERROR, "Things went fine" );
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
        case zyppng::NetworkRequestError::NotFound: // TODO implement retry on different mirror
        case zyppng::NetworkRequestError::Http2Error:
        case zyppng::NetworkRequestError::Http2StreamError: {
          _parent._report->fileDone( req.url(), media::CommitPreloadReport::ERROR, req.extendedErrorString());
          break;
        }
        case zyppng::NetworkRequestError::Unauthorized:
        case zyppng::NetworkRequestError::AuthFailed: {

          //in case we got a auth hint from the server the error object will contain it
          std::string authHint = error.extraInfoValue("authHint", std::string());

          media::CredentialManager cm(media::CredManagerOptions(ZConfig::instance().repoManagerRoot()));
          bool newCreds = media::MediaCurl2::authenticate( req.url(), cm, req.transferSettings(), authHint, _firstAuth );
          if ( newCreds) {
            _firstAuth = false;
            _parent._dispatcher->enqueue( _req );
            return;
          }
          _parent._report->fileDone( req.url(), media::CommitPreloadReport::ACCESS_DENIED, req.extendedErrorString());
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
  int _notFoundRetry = 0;
  ByteCount _lastByteCount = 0;
  Repository::IdType _currentRepoId = sat::detail::noRepoId;

  zyppng::Signal<void()> _sigFinished;

};

CommitPackagePreloader::CommitPackagePreloader()
{}

void CommitPackagePreloader::preloadTransaction( const std::vector<sat::Transaction::Step> &steps)
{
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
    _report->finish();
  };

  // we start a worker for each configured connection
  for ( int i = 0; i < MediaConfig::instance().download_max_concurrent_connections(); i++ ) {
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

void CommitPackagePreloader::reportBytesDownloaded(ByteCount newBytes)
{
  _downloadedBytes += newBytes;
  _pTracker->updateStats( _requiredBytes, _downloadedBytes );
  if ( !_report->progress( _pTracker->_dnlPercent, _pTracker->_drateTotal, _pTracker->_drateLast ) ) {
    //_requiredDls.clear();
    //_dispatcher->cancel();
  }
}

}
