/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include <zypp-curl/ng/network/private/request_p.h>
#include <zypp-curl/ng/network/private/networkrequesterror_p.h>
#include <zypp-curl/ng/network/networkrequestdispatcher.h>
#include <zypp-curl/ng/network/private/mediadebug_p.h>
#include <zypp-core/zyppng/base/EventDispatcher>
#include <zypp-core/zyppng/base/private/linuxhelpers_p.h>
#include <zypp-core/zyppng/core/String>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-curl/private/curlhelper_p.h>
#include <zypp-curl/CurlConfig>
#include <zypp-curl/auth/CurlAuthData>
#include <zypp-media/MediaConfig>
#include <zypp-core/base/String.h>
#include <zypp-core/base/StringV.h>
#include <zypp-core/base/IOTools.h>
#include <zypp-core/Pathname.h>
#include <curl/curl.h>
#include <stdio.h>
#include <fcntl.h>
#include <utility>

#include <iostream>
#include <boost/variant.hpp>
#include <boost/variant/polymorphic_get.hpp>


namespace zyppng {

  namespace  {
    static size_t nwr_headerCallback (  char *ptr, size_t size, size_t nmemb, void *userdata  ) {
      if ( !userdata )
        return 0;

      NetworkRequestPrivate *that = reinterpret_cast<NetworkRequestPrivate *>( userdata );
      return that->headerfunction( ptr, size * nmemb );
    }
    static size_t nwr_writeCallback ( char *ptr, size_t size, size_t nmemb, void *userdata ) {
      if ( !userdata )
        return 0;

      NetworkRequestPrivate *that = reinterpret_cast<NetworkRequestPrivate *>( userdata );
      return that->writefunction( ptr, {}, size * nmemb );
    }

    //helper for std::visit
    template<class T> struct always_false : std::false_type {};
  }

  NetworkRequestPrivate::prepareNextRangeBatch_t::prepareNextRangeBatch_t(running_t &&prevState)
    : _outFile( std::move(prevState._outFile) )
    , _downloaded( prevState._downloaded )
    , _partialHelper( std::move(prevState._partialHelper) )
  { }

  NetworkRequestPrivate::running_t::running_t( pending_t &&prevState )
    : _partialHelper( std::move(prevState._partialHelper) )
  { }

  NetworkRequestPrivate::running_t::running_t( prepareNextRangeBatch_t &&prevState )
    : _outFile( std::move(prevState._outFile) )
    , _partialHelper( std::move(prevState._partialHelper) )
    , _downloaded( prevState._downloaded )
  { }

  NetworkRequestPrivate::NetworkRequestPrivate(Url &&url, zypp::Pathname &&targetFile, NetworkRequest::FileMode fMode , NetworkRequest &p)
    : BasePrivate(p)
    , _url ( std::move(url) )
    , _targetFile ( std::move( targetFile) )
    , _fMode ( std::move(fMode) )
    , _headers( std::unique_ptr< curl_slist, decltype (&curl_slist_free_all) >( nullptr, &curl_slist_free_all ) )
  { }

  NetworkRequestPrivate::~NetworkRequestPrivate()
  {
    if ( _easyHandle ) {
      //clean up for now, later we might reuse handles
      curl_easy_cleanup( _easyHandle );
      //reset in request but make sure the request was not enqueued again and got a new handle
      _easyHandle = nullptr;
    }
  }

  bool NetworkRequestPrivate::initialize( std::string &errBuf )
  {
    reset();

    if ( _easyHandle )
      //will reset to defaults but keep live connections, session ID and DNS caches
      curl_easy_reset( _easyHandle );
    else
      _easyHandle = curl_easy_init();
    return setupHandle ( errBuf );
  }

  bool NetworkRequestPrivate::setupHandle( std::string &errBuf )
  {
    ::internal::setupZYPP_MEDIA_CURL_DEBUG( _easyHandle );
    curl_easy_setopt( _easyHandle, CURLOPT_ERRORBUFFER, this->_errorBuf.data() );

    const std::string urlScheme = _url.getScheme();
    if ( urlScheme == "http" ||  urlScheme == "https" )
      _protocolMode = ProtocolMode::HTTP;

    try {

      setCurlOption( CURLOPT_PRIVATE, this );
      setCurlOption( CURLOPT_XFERINFOFUNCTION, NetworkRequestPrivate::curlProgressCallback );
      setCurlOption( CURLOPT_XFERINFODATA, this  );
      setCurlOption( CURLOPT_NOPROGRESS, 0L);
      setCurlOption( CURLOPT_FAILONERROR, 1L);
      setCurlOption( CURLOPT_NOSIGNAL, 1L);

      std::string urlBuffer( _url.asString() );
      setCurlOption( CURLOPT_URL, urlBuffer.c_str() );

      setCurlOption( CURLOPT_WRITEFUNCTION, nwr_writeCallback );
      setCurlOption( CURLOPT_WRITEDATA, this );

      if ( _options & NetworkRequest::ConnectionTest ) {
        setCurlOption( CURLOPT_CONNECT_ONLY, 1L );
        setCurlOption( CURLOPT_FRESH_CONNECT, 1L );
      }
      if ( _options & NetworkRequest::HeadRequest ) {
        // instead of returning no data with NOBODY, we return
        // little data, that works with broken servers, and
        // works for ftp as well, because retrieving only headers
        // ftp will return always OK code ?
        // See http://curl.haxx.se/docs/knownbugs.html #58
        if (  _protocolMode == ProtocolMode::HTTP && _settings.headRequestsAllowed() )
          setCurlOption( CURLOPT_NOBODY, 1L );
        else
          setCurlOption( CURLOPT_RANGE, "0-1" );
      }

      //make a local copy of the settings, so headers are not added multiple times
      TransferSettings locSet = _settings;

      if ( _dispatcher ) {
        locSet.setUserAgentString( _dispatcher->agentString().c_str() );

        // add custom headers as configured (bsc#955801)
        const auto &cHeaders = _dispatcher->hostSpecificHeaders();
        if ( auto i = cHeaders.find(_url.getHost()); i != cHeaders.end() ) {
          for ( const auto &[key, value] : i->second ) {
            locSet.addHeader( zypp::str::trim( zypp::str::form(
              "%s: %s", key.c_str(), value.c_str() )
            ));
          }
        }
      }

      locSet.addHeader("Pragma:");

      /** Force IPv4/v6 */
      switch ( zypp::env::ZYPP_MEDIA_CURL_IPRESOLVE() )
      {
        case 4: setCurlOption( CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4 ); break;
        case 6: setCurlOption( CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6 ); break;
        default: break;
      }

      setCurlOption( CURLOPT_HEADERFUNCTION, &nwr_headerCallback );
      setCurlOption( CURLOPT_HEADERDATA, this );

      /**
        * Connect timeout
        */
      setCurlOption( CURLOPT_CONNECTTIMEOUT, locSet.connectTimeout() );
      // If a transfer timeout is set, also set CURLOPT_TIMEOUT to an upper limit
      // just in case curl does not trigger its progress callback frequently
      // enough.
      if ( locSet.timeout() )
      {
        setCurlOption( CURLOPT_TIMEOUT, 3600L );
      }

      if ( urlScheme == "https" )
      {
        if ( :: internal::setCurlRedirProtocols ( _easyHandle ) != CURLE_OK ) {
          ZYPP_THROW( zypp::media::MediaCurlSetOptException( _url, _errorBuf.data() ) );
        }

        if( locSet.verifyPeerEnabled() ||
             locSet.verifyHostEnabled() )
        {
          setCurlOption(CURLOPT_CAPATH, locSet.certificateAuthoritiesPath().c_str());
        }

        if( ! locSet.clientCertificatePath().empty() )
        {
          setCurlOption(CURLOPT_SSLCERT, locSet.clientCertificatePath().c_str());
        }
        if( ! locSet.clientKeyPath().empty() )
        {
          setCurlOption(CURLOPT_SSLKEY, locSet.clientKeyPath().c_str());
        }

#ifdef CURLSSLOPT_ALLOW_BEAST
        // see bnc#779177
        setCurlOption( CURLOPT_SSL_OPTIONS, CURLSSLOPT_ALLOW_BEAST );
#endif
        setCurlOption(CURLOPT_SSL_VERIFYPEER, locSet.verifyPeerEnabled() ? 1L : 0L);
        setCurlOption(CURLOPT_SSL_VERIFYHOST, locSet.verifyHostEnabled() ? 2L : 0L);
        // bnc#903405 - POODLE: libzypp should only talk TLS
        setCurlOption(CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
      }

      // follow any Location: header that the server sends as part of
      // an HTTP header (#113275)
      setCurlOption( CURLOPT_FOLLOWLOCATION, 1L);
      // 3 redirects seem to be too few in some cases (bnc #465532)
      setCurlOption( CURLOPT_MAXREDIRS, 6L );

      //set the user agent
      setCurlOption(CURLOPT_USERAGENT, locSet.userAgentString().c_str() );


      /*---------------------------------------------------------------*
        CURLOPT_USERPWD: [user name]:[password]
        Url::username/password -> CURLOPT_USERPWD
        If not provided, anonymous FTP identification
      *---------------------------------------------------------------*/
      if ( locSet.userPassword().size() )
      {
        setCurlOption(CURLOPT_USERPWD, locSet.userPassword().c_str());
        std::string use_auth = _settings.authType();
        if (use_auth.empty())
          use_auth = "digest,basic";	// our default
        long auth = zypp::media::CurlAuthData::auth_type_str2long(use_auth);
        if( auth != CURLAUTH_NONE)
        {
          DBG << _easyHandle << " "  << "Enabling HTTP authentication methods: " << use_auth
              << " (CURLOPT_HTTPAUTH=" << auth << ")" << std::endl;
          setCurlOption(CURLOPT_HTTPAUTH, auth);
        }
      }

      if ( locSet.proxyEnabled() && ! locSet.proxy().empty() )
      {
        DBG << _easyHandle << " " << "Proxy: '" << locSet.proxy() << "'" << std::endl;
        setCurlOption(CURLOPT_PROXY, locSet.proxy().c_str());
        setCurlOption(CURLOPT_PROXYAUTH, CURLAUTH_BASIC|CURLAUTH_DIGEST|CURLAUTH_NTLM );

        /*---------------------------------------------------------------*
         *    CURLOPT_PROXYUSERPWD: [user name]:[password]
         *
         * Url::option(proxyuser and proxypassword) -> CURLOPT_PROXYUSERPWD
         *  If not provided, $HOME/.curlrc is evaluated
         *---------------------------------------------------------------*/

        std::string proxyuserpwd = locSet.proxyUserPassword();

        if ( proxyuserpwd.empty() )
        {
          zypp::media::CurlConfig curlconf;
          zypp::media::CurlConfig::parseConfig(curlconf); // parse ~/.curlrc
          if ( curlconf.proxyuserpwd.empty() )
            DBG << _easyHandle << " "  << "Proxy: ~/.curlrc does not contain the proxy-user option" << std::endl;
          else
          {
            proxyuserpwd = curlconf.proxyuserpwd;
            DBG << _easyHandle << " " << "Proxy: using proxy-user from ~/.curlrc" << std::endl;
          }
        }
        else
        {
          DBG << _easyHandle << " "  << _easyHandle << " "  << "Proxy: using provided proxy-user '" << _settings.proxyUsername() << "'" << std::endl;
        }

        if ( ! proxyuserpwd.empty() )
        {
          setCurlOption(CURLOPT_PROXYUSERPWD, ::internal::curlUnEscape( proxyuserpwd ).c_str());
        }
      }
#if CURLVERSION_AT_LEAST(7,19,4)
      else if ( locSet.proxy() == EXPLICITLY_NO_PROXY )
      {
        // Explicitly disabled in URL (see fillSettingsFromUrl()).
        // This should also prevent libcurl from looking into the environment.
        DBG << _easyHandle << " "  << "Proxy: explicitly NOPROXY" << std::endl;
        setCurlOption(CURLOPT_NOPROXY, "*");
      }

#endif
      // else: Proxy: not explicitly set; libcurl may look into the environment

      /** Speed limits */
      if ( locSet.minDownloadSpeed() != 0 )
      {
        setCurlOption(CURLOPT_LOW_SPEED_LIMIT, locSet.minDownloadSpeed());
        // default to 10 seconds at low speed
        setCurlOption(CURLOPT_LOW_SPEED_TIME, 60L);
      }

#if CURLVERSION_AT_LEAST(7,15,5)
      if ( locSet.maxDownloadSpeed() != 0 )
        setCurlOption(CURLOPT_MAX_RECV_SPEED_LARGE, locSet.maxDownloadSpeed());
#endif

      zypp::filesystem::assert_file_mode( _currentCookieFile, 0600 );
      if ( locSet.cookieFileEnabled() )
        setCurlOption( CURLOPT_COOKIEFILE, _currentCookieFile.c_str() );
      else
        MIL << _easyHandle << " " << "No cookies requested" << std::endl;
      setCurlOption(CURLOPT_COOKIEJAR, _currentCookieFile.c_str() );

#if CURLVERSION_AT_LEAST(7,18,0)
      // bnc #306272
      setCurlOption(CURLOPT_PROXY_TRANSFER_MODE, 1L );
#endif

      // Append settings custom headers to curl.
      // TransferSettings assert strings are trimmed (HTTP/2 RFC 9113)
      for ( const auto &header : locSet.headers() ) {
        if ( !z_func()->addRequestHeader( header.c_str() ) )
          ZYPP_THROW(zypp::media::MediaCurlInitException(_url));
      }

      if ( _headers )
        setCurlOption( CURLOPT_HTTPHEADER, _headers.get() );

      // set up ranges if required
      if( !( _options & NetworkRequest::ConnectionTest ) && !( _options & NetworkRequest::HeadRequest ) ){
        if ( _requestedRanges.size() ) {

          std::sort( _requestedRanges.begin(), _requestedRanges.end(), []( const auto &elem1, const auto &elem2 ){
            return ( elem1._start < elem2._start );
          });

          CurlMultiPartHandler *helper = nullptr;
          if ( auto initState = std::get_if<pending_t>(&_runningMode) ) {

            auto multiPartMode = _protocolMode == ProtocolMode::HTTP ? CurlMultiPartHandler::ProtocolMode::HTTP : CurlMultiPartHandler::ProtocolMode::Basic;
            initState->_partialHelper = std::make_unique<CurlMultiPartHandler>(
                  multiPartMode
                  , _easyHandle
                  , _requestedRanges
                  , *this
            );
            helper = initState->_partialHelper.get();

          } else if ( auto pendingState = std::get_if<prepareNextRangeBatch_t>(&_runningMode) ) {
            helper = pendingState->_partialHelper.get();
          } else {
            errBuf = "Request is in invalid state to call setupHandle";
            return false;
          }

          if ( !helper->prepare () ) {
            errBuf = helper->lastErrorMessage ();
            return false;
          }
        }
      }

      return true;

    } catch ( const zypp::Exception &excp ) {
      ZYPP_CAUGHT(excp);
      errBuf = excp.asString();
    }
    return false;
  }

  bool NetworkRequestPrivate::assertOutputFile()
  {
    auto rmode = std::get_if<NetworkRequestPrivate::running_t>( &_runningMode );
    if ( !rmode ) {
      DBG << _easyHandle << "Can only create output file in running mode" << std::endl;
      return false;
    }
    // if we have no open file create or open it
    if ( !rmode->_outFile ) {
      std::string openMode = "w+b";
      if ( _fMode == NetworkRequest::WriteShared )
        openMode = "r+b";

      rmode->_outFile = fopen( _targetFile.asString().c_str() , openMode.c_str() );

      //if the file does not exist create a new one
      if ( !rmode->_outFile && _fMode == NetworkRequest::WriteShared ) {
        rmode->_outFile = fopen( _targetFile.asString().c_str() , "w+b" );
      }

      if ( !rmode->_outFile ) {
        rmode->_cachedResult = NetworkRequestErrorPrivate::customError(  NetworkRequestError::InternalError
          ,zypp::str::Format("Unable to open target file (%1%). Errno: (%2%:%3%)") % _targetFile.asString() % errno % strerr_cxx() );
        return false;
      }
    }

    return true;
  }

  bool NetworkRequestPrivate::canRecover() const
  {
    // We can recover from RangeFail errors if the helper indicates it
    auto rmode = std::get_if<NetworkRequestPrivate::running_t>( &_runningMode );
    if ( rmode->_partialHelper ) return rmode->_partialHelper->canRecover();
    return false;
  }

  bool NetworkRequestPrivate::prepareToContinue( std::string &errBuf )
  {
    auto rmode = std::get_if<NetworkRequestPrivate::running_t>( &_runningMode );
    if ( !rmode ) {
      errBuf = "NetworkRequestPrivate::prepareToContinue called in invalid state";
      return false;
    }

    if ( rmode->_partialHelper && rmode->_partialHelper->hasMoreWork() ) {

      bool hadRangeFail = rmode->_partialHelper->lastError () == NetworkRequestError::RangeFail;

      _runningMode = prepareNextRangeBatch_t( std::move(std::get<running_t>( _runningMode )) );

      auto &prepMode = std::get<prepareNextRangeBatch_t>(_runningMode);
      if ( !prepMode._partialHelper->prepareToContinue() ) {
        errBuf = prepMode._partialHelper->lastErrorMessage();
        return false;
      }

      if ( hadRangeFail ) {
        // we reset the handle to default values. We do this to not run into
        // "transfer closed with outstanding read data remaining" error CURL sometimes returns when
        // we cancel a connection because of a range error to request a smaller batch.
        // The error will still happen but much less frequently than without resetting the handle.
        //
        // Note: Even creating a new handle will NOT fix the issue
        curl_easy_reset( _easyHandle );
      }
      if ( !setupHandle(errBuf))
        return false;

      return true;
    }
    errBuf = "Request has no more work";
    return false;

  }

  bool NetworkRequestPrivate::hasMoreWork() const
  {
    // check if we have ranges that have never been requested
    return std::any_of( _requestedRanges.begin(), _requestedRanges.end(), []( const auto &range ){ return range._rangeState == CurlMultiPartHandler::Pending; });
  }

  void NetworkRequestPrivate::aboutToStart( )
  {
    bool isRangeContinuation = std::holds_alternative<prepareNextRangeBatch_t>( _runningMode );
    if ( isRangeContinuation ) {
      MIL << _easyHandle << " " << "Continuing a previously started range batch." << std::endl;
      _runningMode = running_t( std::move(std::get<prepareNextRangeBatch_t>( _runningMode )) );
    } else {
      _runningMode = running_t( std::move(std::get<pending_t>( _runningMode )) );
    }

    auto &m = std::get<running_t>( _runningMode );

    if ( m._activityTimer ) {
      DBG_MEDIA << _easyHandle << " Setting activity timeout to: " << _settings.timeout() << std::endl;
      m._activityTimer->connect( &Timer::sigExpired, *this, &NetworkRequestPrivate::onActivityTimeout );
      m._activityTimer->start( static_cast<uint64_t>( _settings.timeout() * 1000 ) );
    }

    if ( !isRangeContinuation )
      _sigStarted.emit( *z_func() );
  }

  void NetworkRequestPrivate::dequeueNotify()
  {
    if ( std::holds_alternative<running_t>(_runningMode) ) {
      auto &rmode = std::get<running_t>( _runningMode );
      if ( rmode._partialHelper )
        rmode._partialHelper->finalize();
    }
  }

  void NetworkRequestPrivate::setResult( NetworkRequestError &&err )
  {
    finished_t resState;
    resState._result = std::move(err);

    if ( std::holds_alternative<running_t>(_runningMode) ) {

      auto &rmode = std::get<running_t>( _runningMode );
      resState._downloaded = rmode._downloaded;
      resState._contentLenght = rmode._contentLenght;

      if ( resState._result.type() == NetworkRequestError::NoError && !(_options & NetworkRequest::HeadRequest) && !(_options & NetworkRequest::ConnectionTest) ) {
        if ( _requestedRanges.size( ) ) {
          //we have a successful download lets see if we got everything we needed
          if ( !rmode._partialHelper->verifyData() ){
            NetworkRequestError::Type err = rmode._partialHelper->lastError();
            resState._result = NetworkRequestErrorPrivate::customError( err, std::string(rmode._partialHelper->lastErrorMessage()) );
          }

          // if we have ranges we need to fill our digest from the full file
          if ( _fileVerification && resState._result.type() == NetworkRequestError::NoError ) {
            if ( fseek( rmode._outFile, 0, SEEK_SET ) != 0 ) {
              resState._result = NetworkRequestErrorPrivate::customError(  NetworkRequestError::InternalError, "Unable to set output file pointer." );
            } else {
              constexpr size_t bufSize = 4096;
              char buf[bufSize];
              while( auto cnt = fread(buf, 1, bufSize, rmode._outFile ) > 0 ) {
                _fileVerification->_fileDigest.update(buf, cnt);
              }
            }
          }
        } // if ( _requestedRanges.size( ) )
      }

      // finally check the file digest if we have one
      if ( _fileVerification && resState._result.type() == NetworkRequestError::NoError ) {
        const UByteArray &calcSum = _fileVerification->_fileDigest.digestVector ();
        const UByteArray &expSum  = zypp::Digest::hexStringToUByteArray( _fileVerification->_fileChecksum.checksum () );
        if ( calcSum != expSum  ) {
             resState._result = NetworkRequestErrorPrivate::customError(
                   NetworkRequestError::InvalidChecksum
                   , (zypp::str::Format("Invalid file checksum %1%, expected checksum %2%")
                      % _fileVerification->_fileDigest.digest()
                      % _fileVerification->_fileChecksum.checksum () ) );
        }
      }

      rmode._outFile.reset();
    }

    _runningMode = std::move( resState );
    _sigFinished.emit( *z_func(), std::get<finished_t>(_runningMode)._result );
  }

  void NetworkRequestPrivate::reset()
  {
    _protocolMode = ProtocolMode::Default;
    _headers.reset( nullptr );
    _errorBuf.fill( 0 );
    _runningMode = pending_t();

    if ( _fileVerification )
      _fileVerification->_fileDigest.reset ();

    std::for_each( _requestedRanges.begin (), _requestedRanges.end(), []( CurlMultiPartHandler::Range &range ) {
        range.restart();
    });
  }

  void NetworkRequestPrivate::onActivityTimeout( Timer &t )
  {
    MIL_MEDIA << _easyHandle << " Request timeout interval: " << t.interval()<< " remaining: " << t.remaining() <<  std::endl;
    std::map<std::string, boost::any> extraInfo;
    extraInfo.insert( {"requestUrl", _url } );
    extraInfo.insert( {"filepath", _targetFile } );
    _dispatcher->cancel( *z_func(), NetworkRequestErrorPrivate::customError( NetworkRequestError::Timeout, "Download timed out", std::move(extraInfo) ) );
  }

  std::string NetworkRequestPrivate::errorMessage() const
  {
    return std::string( _errorBuf.data() );
  }

  void NetworkRequestPrivate::resetActivityTimer()
  {
    if ( std::holds_alternative<running_t>( _runningMode ) ){
      auto &rmode = std::get<running_t>( _runningMode );
      if ( rmode._activityTimer && rmode._activityTimer->isRunning() )
        rmode._activityTimer->start();
    }
  }

  int NetworkRequestPrivate::curlProgressCallback( void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow )
  {
    if ( !clientp )
      return CURLE_OK;
    NetworkRequestPrivate *that = reinterpret_cast<NetworkRequestPrivate *>( clientp );

    if ( !std::holds_alternative<running_t>(that->_runningMode) ){
      DBG << that->_easyHandle << " " << "Curl progress callback was called in invalid state "<< that->z_func()->state() << std::endl;
      return -1;
    }

    auto &rmode = std::get<running_t>( that->_runningMode );

    //reset the timer
    that->resetActivityTimer();

    rmode._isInCallback = true;
    if ( rmode._lastProgressNow != dlnow ) {
      rmode._lastProgressNow = dlnow;
      that->_sigProgress.emit( *that->z_func(), dltotal, dlnow, ultotal, ulnow );
    }
    rmode._isInCallback = false;

    return rmode._cachedResult ? CURLE_ABORTED_BY_CALLBACK : CURLE_OK;
  }

  size_t NetworkRequestPrivate::headerfunction( char *ptr, size_t bytes )
  {
    //it is valid to call this function with no data to write, just return OK
    if ( bytes == 0)
      return 0;

    resetActivityTimer();

    if ( _protocolMode == ProtocolMode::HTTP ) {

      std::string_view hdr( ptr, bytes );

      hdr.remove_prefix( std::min( hdr.find_first_not_of(" \t\r\n"), hdr.size() ) );
      const auto lastNonWhitespace = hdr.find_last_not_of(" \t\r\n");
      if ( lastNonWhitespace != hdr.npos )
        hdr.remove_suffix( hdr.size() - (lastNonWhitespace + 1) );
      else
        hdr = std::string_view();

      auto &rmode = std::get<running_t>( _runningMode );
      if ( !hdr.size() ) {
        return ( bytes );
      }
      if ( _expectedFileSize && rmode._partialHelper ) {
        const auto &repSize = rmode._partialHelper->reportedFileSize ();
        if ( repSize && repSize != _expectedFileSize ) {
          rmode._cachedResult = NetworkRequestErrorPrivate::customError(  NetworkRequestError::InternalError, "Reported downloaded data length is not what was expected." );
          return 0;
        }
      }
      if ( zypp::strv::hasPrefixCI( hdr, "HTTP/" ) ) {

        long statuscode = 0;
        (void)curl_easy_getinfo( _easyHandle, CURLINFO_RESPONSE_CODE, &statuscode);

        // if we have a status 204 we need to create a empty file
        if( statuscode == 204 && !( _options & NetworkRequest::ConnectionTest ) && !( _options & NetworkRequest::HeadRequest ) )
          assertOutputFile();

      } else if ( zypp::strv::hasPrefixCI( hdr, "Location:" ) ) {
        _lastRedirect = hdr.substr( 9 );
        DBG << _easyHandle << " " << "redirecting to " << _lastRedirect << std::endl;

      } else if ( zypp::strv::hasPrefixCI( hdr, "Content-Length:") )  {
        auto lenStr = str::trim( hdr.substr( 15 ), zypp::str::TRIM );
        auto str = std::string ( lenStr.data(), lenStr.length() );
        auto len = zypp::str::strtonum<typename zypp::ByteCount::SizeType>( str.data() );
        if ( len > 0 ) {
          DBG << _easyHandle << " " << "Got Content-Length Header: " << len << std::endl;
          rmode._contentLenght = zypp::ByteCount(len, zypp::ByteCount::B);
        }
      }
    }

    return bytes;
  }

  size_t NetworkRequestPrivate::writefunction( char *data, std::optional<off_t> offset, size_t max )
  {
    //it is valid to call this function with no data to write, just return OK
    if ( max == 0)
      return 0;

    resetActivityTimer();

    //in case of a HEAD request, we do not write anything
    if ( _options & NetworkRequest::HeadRequest ) {
      return ( max );
    }

    auto &rmode = std::get<running_t>( _runningMode );

    // if we have no open file create or open it
    if ( !assertOutputFile() )
      return 0;

    if ( offset ) {
      // seek to the given offset
      if ( fseek( rmode._outFile, *offset, SEEK_SET ) != 0 ) {
        rmode._cachedResult = NetworkRequestErrorPrivate::customError(  NetworkRequestError::InternalError,
          "Unable to set output file pointer." );
        return 0;
      }
      rmode._currentFileOffset = *offset;
    }

    if ( _expectedFileSize && rmode._partialHelper ) {
      const auto &repSize = rmode._partialHelper->reportedFileSize ();
      if ( repSize && repSize != _expectedFileSize ) {
        rmode._cachedResult = NetworkRequestErrorPrivate::customError(  NetworkRequestError::InternalError, "Reported downloaded data length is not what was expected." );
        return 0;
      }
    }

    //make sure we do not write after the expected file size
    if ( _expectedFileSize && static_cast<zypp::ByteCount::SizeType>( rmode._currentFileOffset + max) > _expectedFileSize ) {
      rmode._cachedResult = NetworkRequestErrorPrivate::customError(  NetworkRequestError::ExceededMaxLen, "Downloaded data exceeds expected length." );
      return 0;
    }

    auto written = fwrite( data, 1, max, rmode._outFile );
    if ( written == 0 )
      return 0;

    // if we are not downloading in ranges, we can update the file digest on the fly if we have one
    if ( !rmode._partialHelper && _fileVerification ) {
      _fileVerification->_fileDigest.update( data, written );
    }

    rmode._currentFileOffset += written;

    // count the number of real bytes we have downloaded so far
    rmode._downloaded += written;
    _sigBytesDownloaded.emit( *z_func(), rmode._downloaded );

    return written;
  }

  void NetworkRequestPrivate::notifyErrorCodeChanged()
  {
    auto rmode = std::get_if<NetworkRequestPrivate::running_t>( &_runningMode );
    if ( !rmode || !rmode->_partialHelper || !rmode->_partialHelper->hasError() )
      return;

    // oldest cached result wins
    if ( rmode->_cachedResult )
      return;

    auto lastErr = NetworkRequestErrorPrivate::customError( rmode->_partialHelper->lastError() , std::string(rmode->_partialHelper->lastErrorMessage()) );
    MIL_MEDIA << _easyHandle << " Multipart handler announced error code " << lastErr.toString () <<  std::endl;
    rmode->_cachedResult = lastErr;
  }

  ZYPP_IMPL_PRIVATE(NetworkRequest)

  NetworkRequest::NetworkRequest(zyppng::Url url, zypp::filesystem::Pathname targetFile, zyppng::NetworkRequest::FileMode fMode)
    : Base ( *new NetworkRequestPrivate( std::move(url), std::move(targetFile), std::move(fMode), *this ) )
  {
  }

  NetworkRequest::~NetworkRequest()
  {
    Z_D();

    if ( d->_dispatcher )
      d->_dispatcher->cancel( *this, "Request destroyed while still running" );
  }

  void NetworkRequest::setExpectedFileSize( zypp::ByteCount expectedFileSize )
  {
    d_func()->_expectedFileSize = std::move( expectedFileSize );
  }

  zypp::ByteCount NetworkRequest::expectedFileSize() const
  {
    return d_func()->_expectedFileSize;
  }

  void NetworkRequest::setPriority( NetworkRequest::Priority prio, bool triggerReschedule )
  {
    Z_D();
    d->_priority = prio;
    if ( state() == Pending && triggerReschedule && d->_dispatcher )
      d->_dispatcher->reschedule();
  }

  NetworkRequest::Priority NetworkRequest::priority() const
  {
    return d_func()->_priority;
  }

  void NetworkRequest::setOptions( Options opt )
  {
    d_func()->_options = opt;
  }

  NetworkRequest::Options NetworkRequest::options() const
  {
    return d_func()->_options;
  }

  void NetworkRequest::addRequestRange(size_t start, size_t len, std::optional<zypp::Digest> &&digest, CheckSumBytes expectedChkSum , std::any userData, std::optional<size_t> digestCompareLen, std::optional<size_t> chksumpad  )
  {
    Z_D();
    if ( state() == Running )
      return;

    d->_requestedRanges.push_back( Range::make( start, len, std::move(digest), std::move( expectedChkSum ), std::move( userData ), digestCompareLen, chksumpad ) );
  }

  void NetworkRequest::addRequestRange(zyppng::NetworkRequest::Range &&range )
  {
    Z_D();
    if ( state() == Running )
      return;

    d->_requestedRanges.push_back( std::move(range) );
    auto &rng = d->_requestedRanges.back();
    rng._rangeState = CurlMultiPartHandler::Pending;
    rng.bytesWritten = 0;
    if ( rng._digest )
      rng._digest->reset();
  }

  bool NetworkRequest::setExpectedFileChecksum( const zypp::CheckSum &expected )
  {
    Z_D();
    if ( state() == Running )
      return false;

    zypp::Digest fDig;
    if ( !fDig.create( expected.type () ) )
      return false;

    d->_fileVerification = NetworkRequestPrivate::FileVerifyInfo{
        ._fileDigest   = std::move(fDig),
        ._fileChecksum = expected
    };
    return true;
  }

  void NetworkRequest::resetRequestRanges()
  {
    Z_D();
    if ( state() == Running )
      return;
    d->_requestedRanges.clear();
  }

  std::vector<NetworkRequest::Range> NetworkRequest::failedRanges() const
  {
    const auto mystate = state();
    if ( mystate != Finished && mystate != Error )
      return {};

    Z_D();

    std::vector<Range> failed;
    for ( auto &r : d->_requestedRanges ) {
      if ( r._rangeState != CurlMultiPartHandler::Finished ) {
        failed.push_back( r.clone() );
      }
    }
    return failed;
  }

  const std::vector<NetworkRequest::Range> &NetworkRequest::requestedRanges() const
  {
    return d_func()->_requestedRanges;
  }

  const std::string &NetworkRequest::lastRedirectInfo() const
  {
    return d_func()->_lastRedirect;
  }

  void *NetworkRequest::nativeHandle() const
  {
    return d_func()->_easyHandle;
  }

  std::optional<zyppng::NetworkRequest::Timings> NetworkRequest::timings() const
  {
    const auto myerr = error();
    const auto mystate = state();
    if ( mystate != Finished )
      return {};

    Timings t;

    auto getMeasurement = [ this ]( const CURLINFO info, std::chrono::microseconds &target ){
      using FPSeconds = std::chrono::duration<double, std::chrono::seconds::period>;
      double val = 0;
      const auto res = curl_easy_getinfo( d_func()->_easyHandle, info, &val );
      if ( CURLE_OK == res ) {
        target = std::chrono::duration_cast<std::chrono::microseconds>( FPSeconds(val) );
      }
    };

    getMeasurement( CURLINFO_NAMELOOKUP_TIME, t.namelookup );
    getMeasurement( CURLINFO_CONNECT_TIME, t.connect);
    getMeasurement( CURLINFO_APPCONNECT_TIME, t.appconnect);
    getMeasurement( CURLINFO_PRETRANSFER_TIME , t.pretransfer);
    getMeasurement( CURLINFO_TOTAL_TIME, t.total);
    getMeasurement( CURLINFO_REDIRECT_TIME, t.redirect);

    return t;
  }

  std::vector<char> NetworkRequest::peekData( off_t offset, size_t count ) const
  {
    Z_D();

    if ( !std::holds_alternative<NetworkRequestPrivate::running_t>( d->_runningMode) )
      return {};

    const auto &rmode = std::get<NetworkRequestPrivate::running_t>( d->_runningMode );
    return zypp::io::peek_data_fd( rmode._outFile, offset, count );
  }

  Url NetworkRequest::url() const
  {
    return d_func()->_url;
  }

  void NetworkRequest::setUrl(const Url &url)
  {
    Z_D();
    if ( state() == NetworkRequest::Running )
      return;

    d->_url = url;
  }

  const zypp::filesystem::Pathname &NetworkRequest::targetFilePath() const
  {
    return d_func()->_targetFile;
  }

  void NetworkRequest::setTargetFilePath( const zypp::filesystem::Pathname &path )
  {
    Z_D();
    if ( state() == NetworkRequest::Running )
      return;
    d->_targetFile = path;
  }

  NetworkRequest::FileMode NetworkRequest::fileOpenMode() const
  {
    return d_func()->_fMode;
  }

  void NetworkRequest::setFileOpenMode( FileMode mode )
  {
    Z_D();
    if ( state() == NetworkRequest::Running )
      return;
    d->_fMode = std::move( mode );
  }

  std::string NetworkRequest::contentType() const
  {
    char *ptr = NULL;
    if ( curl_easy_getinfo( d_func()->_easyHandle, CURLINFO_CONTENT_TYPE, &ptr ) == CURLE_OK && ptr )
      return std::string(ptr);
    return std::string();
  }

  zypp::ByteCount NetworkRequest::reportedByteCount() const
  {
    return std::visit([](auto& arg) -> zypp::ByteCount {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, NetworkRequestPrivate::pending_t> || std::is_same_v<T, NetworkRequestPrivate::prepareNextRangeBatch_t> )
        return zypp::ByteCount(0);
      else if constexpr (std::is_same_v<T, NetworkRequestPrivate::running_t>
                         || std::is_same_v<T, NetworkRequestPrivate::finished_t>)
        return arg._contentLenght;
      else
        static_assert(always_false<T>::value, "Unhandled state type");
    }, d_func()->_runningMode);
  }

  zypp::ByteCount NetworkRequest::downloadedByteCount() const
  {
    return std::visit([](auto& arg) -> zypp::ByteCount {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, NetworkRequestPrivate::pending_t>)
        return zypp::ByteCount();
      else if constexpr (std::is_same_v<T, NetworkRequestPrivate::running_t>
                          || std::is_same_v<T, NetworkRequestPrivate::prepareNextRangeBatch_t>
                          || std::is_same_v<T, NetworkRequestPrivate::finished_t>)
        return arg._downloaded;
      else
        static_assert(always_false<T>::value, "Unhandled state type");
    }, d_func()->_runningMode);
  }

  TransferSettings &NetworkRequest::transferSettings()
  {
    return d_func()->_settings;
  }

  NetworkRequest::State NetworkRequest::state() const
  {
    return std::visit([this](auto& arg) {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, NetworkRequestPrivate::pending_t>)
        return Pending;
      else if constexpr (std::is_same_v<T, NetworkRequestPrivate::running_t> || std::is_same_v<T, NetworkRequestPrivate::prepareNextRangeBatch_t> )
        return Running;
      else if constexpr (std::is_same_v<T, NetworkRequestPrivate::finished_t>) {
        if ( std::get<NetworkRequestPrivate::finished_t>( d_func()->_runningMode )._result.isError() )
          return Error;
        else
          return Finished;
      }
      else
        static_assert(always_false<T>::value, "Unhandled state type");
    }, d_func()->_runningMode);
  }

  NetworkRequestError NetworkRequest::error() const
  {
    const auto s = state();
    if ( s != Error && s != Finished )
      return NetworkRequestError();
    return std::get<NetworkRequestPrivate::finished_t>( d_func()->_runningMode)._result;
  }

  std::string NetworkRequest::extendedErrorString() const
  {
    if ( !hasError() )
      return std::string();

    return error().nativeErrorString();
  }

  bool NetworkRequest::hasError() const
  {
    return error().isError();
  }

  bool NetworkRequest::addRequestHeader( const std::string &header )
  {
    Z_D();

    curl_slist *res = curl_slist_append( d->_headers ? d->_headers.get() : nullptr, header.c_str() );
    if ( !res )
      return false;

    if ( !d->_headers )
      d->_headers = std::unique_ptr< curl_slist, decltype (&curl_slist_free_all) >( res, &curl_slist_free_all );

    return true;
  }

  const zypp::Pathname &NetworkRequest::cookieFile() const
  {
    return d_func()->_currentCookieFile;
  }

  void NetworkRequest::setCookieFile(zypp::Pathname cookieFile )
  {
    d_func()->_currentCookieFile = std::move(cookieFile);
  }

  SignalProxy<void (NetworkRequest &req)> NetworkRequest::sigStarted()
  {
    return d_func()->_sigStarted;
  }

  SignalProxy<void (NetworkRequest &req, zypp::ByteCount count)> NetworkRequest::sigBytesDownloaded()
  {
    return d_func()->_sigBytesDownloaded;
  }

  SignalProxy<void (NetworkRequest &req, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow)> NetworkRequest::sigProgress()
  {
    return d_func()->_sigProgress;
  }

  SignalProxy<void (zyppng::NetworkRequest &req, const zyppng::NetworkRequestError &err)> NetworkRequest::sigFinished()
  {
    return d_func()->_sigFinished;
  }

}
