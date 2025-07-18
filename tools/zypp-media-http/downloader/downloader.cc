/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include <downloader/private/downloader_p.h>
#include <zypp-curl/ng/network/private/mediadebug_p.h>
#include <zypp-curl/ng/network/private/networkrequesterror_p.h>
#include <zypp-curl/ng/network/networkrequestdispatcher.h>
#include <utility>
#include <zypp-curl/TransferSettings>
#include <zypp-curl/private/curlhelper_p.h>
#include <zypp-media/MediaException>
#include <zypp-core/base/String.h>

#if ENABLE_ZCHUNK_COMPRESSION
#include <zypp-curl/ng/network/zckhelper.h>
#endif

namespace zyppng {

  DownloadPrivateBase::DownloadPrivateBase(Downloader &parent, std::shared_ptr<NetworkRequestDispatcher> requestDispatcher, std::shared_ptr<MirrorControl> mirrors, DownloadSpec &&spec, Download &p)
    : BasePrivate(p)
    , _requestDispatcher ( std::move(requestDispatcher) )
    , _mirrorControl( std::move(mirrors) )
    , _spec( std::move(spec) )
    , _parent( &parent )
  {}

  DownloadPrivateBase::~DownloadPrivateBase()
  { }

  bool DownloadPrivateBase::handleRequestAuthError( const std::shared_ptr<Request>& req, const zyppng::NetworkRequestError &err )
  {
    //Handle the auth errors explicitly, we need to give the user a way to put in new credentials
    //if we get valid new credentials we can retry the request
    bool retry = false;
    if ( err.type() == NetworkRequestError::Unauthorized || err.type() == NetworkRequestError::AuthFailed ) {

      MIL << "Authentication failed for " << req->url() << " trying to recover." << std::endl;

      TransferSettings &ts = req->transferSettings();
      const auto &applyCredToSettings = [&ts]( const AuthData_Ptr& auth, const std::string &authHint ) {
        ts.setUsername( auth->username() );
        ts.setPassword( auth->password() );
        auto nwCred = dynamic_cast<NetworkAuthData *>( auth.get() );
        if ( nwCred ) {
          // set available authentication types from the error
          if ( nwCred->authType() == CURLAUTH_NONE )
            nwCred->setAuthType( authHint );

          // set auth type (seems this must be set _after_ setting the userpwd)
          if ( nwCred->authType()  != CURLAUTH_NONE ) {
            // FIXME: only overwrite if not empty?
            ts.setAuthType(nwCred->authTypeAsString());
          }
        }
      };

      // try to find one in the cache
      zypp::url::ViewOption vopt;
      vopt = vopt
             - zypp::url::ViewOption::WITH_USERNAME
             - zypp::url::ViewOption::WITH_PASSWORD
             - zypp::url::ViewOption::WITH_QUERY_STR;

      auto cachedCred = zypp::media::CredentialManager::findIn( _credCache, req->url(), vopt );

      // only consider a cache entry if its newer than what we tried last time
      if ( cachedCred && cachedCred->lastDatabaseUpdate() > req->_authTimestamp ) {
        MIL << "Found a credential match in the cache!" << std::endl;
        applyCredToSettings( cachedCred, "" );
        _lastTriedAuthTime = req->_authTimestamp = cachedCred->lastDatabaseUpdate();
        retry = true;
      } else {

        NetworkAuthData_Ptr credFromUser = NetworkAuthData_Ptr( new NetworkAuthData() );
        credFromUser->setUrl( req->url() );
        credFromUser->setLastDatabaseUpdate ( req->_authTimestamp );

        //in case we got a auth hint from the server the error object will contain it
        std::string authHint = err.extraInfoValue("authHint", std::string());

        _sigAuthRequired.emit( *z_func(), *credFromUser, authHint );
        if ( credFromUser->valid() ) {
          // remember for next time , we don't want to ask the user again for the same URL set
          _credCache.insert( credFromUser );
          applyCredToSettings( credFromUser, authHint );
          _lastTriedAuthTime = req->_authTimestamp = credFromUser->lastDatabaseUpdate();
          retry = true;
        }
      }
    }
    return retry;
  }

#if ENABLE_ZCHUNK_COMPRESSION
  bool DownloadPrivateBase::hasZckInfo() const
  {
    if ( zypp::indeterminate(_specHasZckInfo) )
      _specHasZckInfo = ( _spec.headerSize() > 0 && ZckLoader::isZchunkFile( _spec.deltaFile() ) );
    return bool(_specHasZckInfo);
  }
#endif

  void DownloadPrivateBase::Request::disconnectSignals()
  {
    _sigStartedConn.disconnect();
    _sigProgressConn.disconnect();
    _sigFinishedConn.disconnect();
  }

  DownloadPrivate::DownloadPrivate(Downloader &parent, std::shared_ptr<NetworkRequestDispatcher> requestDispatcher, std::shared_ptr<MirrorControl> mirrors, DownloadSpec &&spec, Download &p)
    : DownloadPrivateBase( parent, std::move(requestDispatcher), std::move(mirrors), std::move(spec), p )
  { }

  void DownloadPrivate::init()
  {
    Base::connectFunc( *this, &DownloadStatemachine<DownloadPrivate>::sigFinished, [this](){
      DownloadPrivateBase::_sigFinished.emit( *z_func() );
    } );

    Base::connectFunc( *this, &DownloadStatemachine<DownloadPrivate>::sigStateChanged, [this]( const auto state ){
      DownloadPrivateBase::_sigStateChanged.emit( *z_func(), state );
    } );
  }

  void DownloadPrivate::start()
  {
    auto cState = currentState();
    if ( !cState )
      DownloadStatemachine<DownloadPrivate>::start();

    cState = currentState();
    if ( *cState != Download::InitialState && *cState != Download::Finished ) {
      // the state machine has advaned already, we can only restart it in a finished state
        return;
    }

    //reset state variables
    _specHasZckInfo    = zypp::indeterminate;
    _emittedSigStart   = false;
    _stoppedOnMetalink = false;
    _lastTriedAuthTime = 0;

    // restart the statemachine
    if ( cState == Download::Finished )
      DownloadStatemachine<DownloadPrivate>::start();

    //jumpstart the process
    state<InitialState>()->initiate();
  }


  NetworkRequestError DownloadPrivateBase::safeFillSettingsFromURL( const Url &url, TransferSettings &set)
  {
    auto buildExtraInfo = [this, &url](){
      std::map<std::string, boost::any> extraInfo;
      extraInfo.insert( {"requestUrl", url } );
      extraInfo.insert( {"filepath", _spec.targetPath() } );
      return extraInfo;
    };

    NetworkRequestError res;
    try {
      ::internal::fillSettingsFromUrl( url, set );
      if ( _spec.settings().proxy().empty() )
        ::internal::fillSettingsSystemProxy( url, set );

#if 0
      /* Fixes bsc#1174011 "auth=basic ignored in some cases"
       * We should proactively add the password to the request if basic auth is configured
       * and a password is available in the credentials but not in the URL.
       *
       * We will be a bit paranoid here and require that the URL has a user embedded, otherwise we go the default route
       * and ask the server first about the auth method
       */
      if ( set.authType() == "basic"
           && set.username().size()
           && !set.password().size() ) {
        zypp::media::CredentialManager cm(  _parent ? _parent->credManagerOptions() : zypp::media::CredManagerOptions() );
        const auto cred = cm.getCred( url );
        if ( cred && cred->valid() ) {
          if ( !set.username().size() )
            set.setUsername(cred->username());
          set.setPassword(cred->password());
        }
      }
#endif

    } catch ( const zypp::media::MediaBadUrlException & e ) {
      res = NetworkRequestErrorPrivate::customError( NetworkRequestError::MalformedURL, e.asString(), buildExtraInfo() );
    } catch ( const zypp::media::MediaUnauthorizedException & e ) {
      res = NetworkRequestErrorPrivate::customError( NetworkRequestError::AuthFailed, e.asString(), buildExtraInfo() );
    } catch ( const zypp::Exception & e ) {
      res = NetworkRequestErrorPrivate::customError( NetworkRequestError::InternalError, e.asString(), buildExtraInfo() );
    }
    return res;
  }

  Download::Download(zyppng::Downloader &parent, std::shared_ptr<zyppng::NetworkRequestDispatcher> requestDispatcher, std::shared_ptr<zyppng::MirrorControl> mirrors, zyppng::DownloadSpec &&spec)
    : Base( *new DownloadPrivate( parent, std::move(requestDispatcher), std::move(mirrors), std::move(spec), *this )  )
  { }

  ZYPP_IMPL_PRIVATE(Download)

  Download::~Download()
  {
    if ( state() != InitialState && state() != Finished )
      cancel();
  }

  Download::State Download::state() const
  {
    const auto &s = d_func()->currentState();
    if ( !s )
      return Download::InitialState;
    return *s;
  }

  NetworkRequestError Download::lastRequestError() const
  {
    if ( state() == Finished ) {
      return d_func()->state<FinishedState>()->_error;
    }
    return NetworkRequestError();
  }

  bool Download::hasError() const
  {
    return lastRequestError().isError();
  }

  std::string Download::errorString() const
  {
    const auto &lReq = lastRequestError();
    if (! lReq.isError() ) {
      return {};
    }

    return ( zypp::str::Format("%1%(%2%)") % lReq.toString() % lReq.nativeErrorString() );
  }

  void Download::start()
  {
    d_func()->start();
  }

  void Download::prioritize()
  {
    Z_D();

    if ( !d->_requestDispatcher )
      return;

    d->_defaultSubRequestPriority = NetworkRequest::Critical;

    // we only reschedule requests when we are in a state that downloads in blocks
    d->visitState( []( auto &s ){
      using T = std::decay_t<decltype (s)>;
      if constexpr ( std::is_same_v<T, DlMetalinkState>
#if ENABLE_ZCHUNK_COMPRESSION
          || std::is_same_v<T, DLZckState>
#endif
      ) {
        s.reschedule();
      }
    });
  }

  void Download::cancel()
  {
    Z_D();
    d->forceState ( std::make_unique<FinishedState>( NetworkRequestErrorPrivate::customError( NetworkRequestError::Cancelled, "Download was cancelled explicitly" ), *d_func() ) );
  }

  void Download::setStopOnMetalink(const bool set)
  {
    d_func()->_stopOnMetalink = set;
  }

  bool Download::stoppedOnMetalink() const
  {
    return d_func()->_stoppedOnMetalink;
  }

  DownloadSpec &Download::spec()
  {
    return d_func()->_spec;
  }

  const DownloadSpec &Download::spec() const
  {
    return d_func()->_spec;
  }

  uint64_t Download::lastAuthTimestamp() const
  {
    return d_func()->_lastTriedAuthTime;
  }

  zyppng::NetworkRequestDispatcher &Download::dispatcher() const
  {
    return *d_func()->_requestDispatcher;
  }

  SignalProxy<void (Download &req)> Download::sigStarted()
  {
    return d_func()->_sigStarted;
  }

  SignalProxy<void (Download &req, Download::State state)> Download::sigStateChanged()
  {
    return d_func()->DownloadPrivateBase::_sigStateChanged;
  }

  SignalProxy<void (zyppng::Download &req, off_t dlnow)> zyppng::Download::sigAlive()
  {
    return d_func()->_sigAlive;
  }

  SignalProxy<void (Download &req, off_t dltotal, off_t dlnow)> Download::sigProgress()
  {
    return d_func()->_sigProgress;
  }

  SignalProxy<void (Download &req)> Download::sigFinished()
  {
    return d_func()->DownloadPrivateBase::_sigFinished;
  }

  SignalProxy<void (zyppng::Download &req, zyppng::NetworkAuthData &auth, const std::string &availAuth)> Download::sigAuthRequired()
  {
    return d_func()->_sigAuthRequired;
  }

  DownloaderPrivate::DownloaderPrivate(std::shared_ptr<MirrorControl> mc, Downloader &p)
    : BasePrivate(p)
    , _mirrors( std::move(mc) )
  {
    _requestDispatcher = std::make_shared<NetworkRequestDispatcher>( );
    if ( !_mirrors ) {
      _mirrors = MirrorControl::create();
    }
  }

  void DownloaderPrivate::onDownloadStarted(Download &download)
  {
    _sigStarted.emit( *z_func(), download );
  }

  void DownloaderPrivate::onDownloadFinished( Download &download )
  {
    _sigFinished.emit( *z_func(), download );

    auto it = std::find_if( _runningDownloads.begin(), _runningDownloads.end(), [ &download ]( const std::shared_ptr<Download> &dl){
      return dl.get() == &download;
    });

    if ( it != _runningDownloads.end() ) {
      //make sure this is not deleted before all user code was done
      _runningDownloads.erase( it );
    }

    if ( _runningDownloads.empty() )
      _queueEmpty.emit( *z_func() );
  }

  ZYPP_IMPL_PRIVATE(Downloader)

  Downloader::Downloader( )
    : Base ( *new DownloaderPrivate( {}, *this ) )
  {

  }

  Downloader::Downloader( std::shared_ptr<MirrorControl> mc )
    : Base ( *new DownloaderPrivate( std::move(mc), *this ) )
  { }

  Downloader::~Downloader()
  {
    Z_D();
    while ( d->_runningDownloads.size() ) {
      d->_runningDownloads.back()->cancel();
      d->_runningDownloads.pop_back();
    }
  }

  std::shared_ptr<Download> Downloader::downloadFile(const zyppng::DownloadSpec &spec )
  {
    Z_D();
    std::shared_ptr<Download> dl ( new Download ( *this, d->_requestDispatcher, d->_mirrors, DownloadSpec(spec) ) );

    d->_runningDownloads.push_back( dl );
    dl->connect( &Download::sigFinished, *d, &DownloaderPrivate::onDownloadFinished );
    d->_requestDispatcher->run();

    return dl;
  }

  std::shared_ptr<NetworkRequestDispatcher> Downloader::requestDispatcher() const
  {
    return d_func()->_requestDispatcher;
  }

  SignalProxy<void (Downloader &parent, Download &download)> Downloader::sigStarted()
  {
    return d_func()->_sigStarted;
  }

  SignalProxy<void (Downloader &parent, Download &download)> Downloader::sigFinished()
  {
    return d_func()->_sigFinished;
  }

  SignalProxy<void (Downloader &parent)> Downloader::queueEmpty()
  {
    return d_func()->_queueEmpty;
  }

}
