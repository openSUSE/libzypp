/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "networkprovider.h"

#include <zypp-curl/ng/network/NetworkRequestDispatcher>
#include <zypp-curl/ng/network/request.h>
#include <zypp-curl/private/curlhelper_p.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/CheckSum.h>
#include <zypp-media/ng/private/providedbg_p.h>
#include <zypp-curl/ng/network/private/networkrequesterror_p.h>


#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "NetworkProvider"


NetworkProvideItem::NetworkProvideItem(NetworkProvider &parent, zyppng::ProvideMessage &&spec)
  : zyppng::worker::ProvideWorkerItem( std::move(spec) )
  , _parent(parent)
{
  const auto &expFilesize     = _spec.value( zyppng::ProvideMsgFields::ExpectedFilesize );
  const auto &headerSize      = _spec.value( zyppng::ProvideMsgFields::FileHeaderSize );
  const auto &checkExistsOnly = _spec.value( zyppng::ProvideMsgFields::CheckExistOnly );
  const auto &deltaFile       = _spec.value( zyppng::ProvideMsgFields::DeltaFile );


  _deltaFile   = deltaFile.isString()  ? zypp::Pathname(deltaFile.asString()) : std::optional<zypp::Pathname>();
  _expFilesize = expFilesize.isInt64() ? std::make_optional<zypp::ByteCount>( expFilesize.asInt64() ) : std::optional<zypp::ByteCount>();
  _headerSize  = headerSize.isInt64 () ? std::make_optional<zypp::ByteCount>( headerSize.asInt64() ) : std::optional<zypp::ByteCount>();
  _checkExistsOnly = ( checkExistsOnly.valid() && checkExistsOnly.isBool() && checkExistsOnly.asBool() );
}

NetworkProvideItem::~NetworkProvideItem()
{
  clearConnections();
  if ( _dl ) {
    _parent._dlManager->cancel ( *_dl );
  }
}

void NetworkProvideItem::startDownload( zypp::Url url )
{
  if ( _state == ProvideWorkerItem::Running || _dl )
    throw std::runtime_error("Can not start a already running Download");

#if 0
  zyppng::DownloadSpec spec(
    url
    , stagingPath
    , expFilesize.valid() ? zypp::ByteCount( expFilesize.asInt64() ) : zypp::ByteCount()
  );
  spec
    .setCheckExistsOnly( checkExistsOnly.valid() ? checkExistsOnly.asBool() : false )
    .setDeltaFile ( deltaFile.valid() ? deltaFile.asString() : zypp::Pathname() );
#endif

  MIL_PRV << "Starting download of : " << url << "with target path: " << _stagingFileName << std::endl;

  clearConnections();

  // we set the state to running immediately but only send the message to controller once the item actually started
  _state = ProvideWorkerItem::Running;

  zyppng::TransferSettings settings;
  const auto &err = safeFillSettingsFromURL ( url, settings );
  if ( err.isError () ) {
    _lastError = err;
    setFinished();
    return;
  }

  _dl = std::make_shared<zyppng::NetworkRequest>( url, _stagingFileName, zyppng::NetworkRequest::WriteShared );
  _dl->transferSettings() = settings;

  if ( _expFilesize )
    _dl->setExpectedFileSize( *_expFilesize );

  _connections.emplace_back( connect( *_dl, &zyppng::NetworkRequest::sigStarted, *this, &NetworkProvideItem::onStarted ) );
  _connections.emplace_back( connect( *_dl, &zyppng::NetworkRequest::sigFinished, *this, &NetworkProvideItem::onFinished ) );

#ifdef ENABLE_ZCHUNK_COMPRESSION
  if ( !_checkExistsOnly
       && _deltaFile
       && zyppng::ZckLoader::isZchunkFile ( _deltaFile.value() )) {

    _zchunkLoader = std::make_shared<zyppng::ZckLoader>();
    _zchunkLoader->connect( &zyppng::ZckLoader::sigBlocksRequired, *this, &NetworkProvideItem::onZckBlocksRequired );
    _zchunkLoader->connect( &zyppng::ZckLoader::sigFinished, *this, &NetworkProvideItem::onZckFinished );
    try {
      _zchunkLoader->buildZchunkFile (
            _stagingFileName,
            _deltaFile.value(),
            _expFilesize,
            _headerSize
      ).unwrap();

      // zck download started
      return;

    } catch( const zypp::Exception &e ) {
      ERR << "Failed to setup zck download: " << e << std::endl;
    }
  }
#endif

  if ( _checkExistsOnly )
    _dl->setOptions ( zyppng::NetworkRequest::HeadRequest );

  normalDownload();
}

zyppng::NetworkRequestError NetworkProvideItem::safeFillSettingsFromURL( zypp::Url &url, zypp::media::TransferSettings &set)
{
  auto buildExtraInfo = [this, &url](){
    std::map<std::string, boost::any> extraInfo;
    extraInfo.insert( {"requestUrl", url } );
    extraInfo.insert( {"filepath", _targetFileName } );
    return extraInfo;
  };

  zyppng::NetworkRequestError res;
  try {
    ::internal::prepareSettingsAndUrl( url, set );
  } catch ( const zypp::media::MediaBadUrlException & e ) {
    res = zyppng::NetworkRequestErrorPrivate::customError( zyppng::NetworkRequestError::MalformedURL, e.asString(), buildExtraInfo() );
  } catch ( const zypp::media::MediaUnauthorizedException & e ) {
    res = zyppng::NetworkRequestErrorPrivate::customError( zyppng::NetworkRequestError::AuthFailed, e.asString(), buildExtraInfo() );
  } catch ( const zypp::Exception & e ) {
    res = zyppng::NetworkRequestErrorPrivate::customError( zyppng::NetworkRequestError::InternalError, e.asString(), buildExtraInfo() );
  }
  return res;
}

void NetworkProvideItem::cancelDownload()
{
  _state = zyppng::worker::ProvideWorkerItem::Finished;
  if ( _dl ) {
    _parent._dlManager->cancel ( *_dl );
    clearConnections();
    _dl.reset();
#ifdef ENABLE_ZCHUNK_COMPRESSION
    _zchunkLoader.reset();
#endif
  }
}

const std::optional<zyppng::NetworkRequestError> &NetworkProvideItem::error() const
{
  return _lastError;
}

void NetworkProvideItem::clearConnections()
{
  for ( auto c : _connections)
    c.disconnect();
  _connections.clear();
}

void NetworkProvideItem::setFinished()
{
  _state = zyppng::worker::ProvideWorkerItem::Finished;
  _parent.itemFinished (shared_this<NetworkProvideItem>() );
}

void NetworkProvideItem::onStarted( zyppng::NetworkRequest & )
{
  if ( _emittedStart )
    return;

  _emittedStart = true;
  _parent.itemStarted( shared_this<NetworkProvideItem>() );
}

void NetworkProvideItem::onFinished( zyppng::NetworkRequest &result, const zyppng::NetworkRequestError & )
{
  if ( result.hasError () ) {

    const auto &err = result.error();
    _lastError = err;

    switch ( err.type () ) {
      case zyppng::NetworkRequestError::NoError:
        // err what
        break;
      case zyppng::NetworkRequestError::Unauthorized:
      case zyppng::NetworkRequestError::AuthFailed: {

        bool retry = false;
        MIL << "Authentication failed for " << _dl->url() << " trying to recover." << std::endl;

        auto &ts = _dl->transferSettings();
        const auto &applyCredToSettings = [&ts]( const zyppng::AuthData_Ptr& auth, const std::string &authHint ) {
          ts.setUsername( auth->username() );
          ts.setPassword( auth->password() );
          auto nwCred = dynamic_cast<zyppng::NetworkAuthData *>( auth.get() );
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

        auto cachedCred = zypp::media::CredentialManager::findIn( _parent._credCache, _dl->url(), vopt );

        // only consider a cache entry if its newer than what we tried last time
        if ( cachedCred && cachedCred->lastDatabaseUpdate() > _authTimestamp ) {
          MIL << "Found a credential match in the cache!" << std::endl;
          applyCredToSettings( cachedCred, "" );
          _authTimestamp = cachedCred->lastDatabaseUpdate();
          retry = true;
        } else {

          zyppng::NetworkAuthData_Ptr credFromUser = zyppng::NetworkAuthData_Ptr( new zyppng::NetworkAuthData() );
          credFromUser->setUrl( _dl->url() );
          credFromUser->setLastDatabaseUpdate ( _authTimestamp );

          //in case we got a auth hint from the server the error object will contain it
          std::string authHint = err.extraInfoValue("authHint", std::string());

          _parent.itemAuthRequired ( shared_this<NetworkProvideItem>(), *credFromUser, authHint );
          if ( credFromUser->valid() ) {
            // remember for next time , we don't want to ask the user again for the same URL set
            _parent._credCache.insert( credFromUser );
            applyCredToSettings( credFromUser, authHint );
            _authTimestamp = credFromUser->lastDatabaseUpdate();
            retry = true;
          }
        }

        if ( retry ) {
          //make sure this request will run asap
          _dl->setPriority( zyppng::NetworkRequest::High );
          _parent._dlManager->enqueue (_dl);
          return;
        }

        // fall through to request failed
      }

      case zyppng::NetworkRequestError::InternalError:
      case zyppng::NetworkRequestError::Cancelled:
      case zyppng::NetworkRequestError::PeerCertificateInvalid:
      case zyppng::NetworkRequestError::ConnectionFailed:
      case zyppng::NetworkRequestError::ExceededMaxLen:
      case zyppng::NetworkRequestError::InvalidChecksum:
      case zyppng::NetworkRequestError::UnsupportedProtocol:
      case zyppng::NetworkRequestError::MalformedURL:
      case zyppng::NetworkRequestError::TemporaryProblem:
      case zyppng::NetworkRequestError::Timeout:
      case zyppng::NetworkRequestError::Forbidden:
      case zyppng::NetworkRequestError::NotFound:
      case zyppng::NetworkRequestError::ServerReturnedError:
      case zyppng::NetworkRequestError::MissingData:
      case zyppng::NetworkRequestError::RangeFail:
      case zyppng::NetworkRequestError::Http2Error:
      case zyppng::NetworkRequestError::Http2StreamError: {

#ifdef ENABLE_ZCHUNK_COMPRESSION
        if ( _zchunkLoader && err.type() == zyppng::NetworkRequestError::RangeFail ) {
          MIL << "Downloading in ranges failed for " << _dl->url() << " trying to recover via normal download." << std::endl;
          normalDownload ();
          return;
        }
#endif

        MIL << _dl->nativeHandle() << " " << "Downloading on " << _dl->url() << " failed with error "<< err.toString() << " " << err.nativeErrorString() << std::endl;
        if ( _dl->lastRedirectInfo ().size () )
          MIL << _dl->nativeHandle() << " Last redirection target was: " << _dl->lastRedirectInfo () << std::endl;

        setFinished();
        return;
      }
    }
  }

#ifdef ENABLE_ZCHUNK_COMPRESSION
  if ( _zchunkLoader ) {
    // if the zckLoader is active, we just tell it to continue if the request has no error
    try {
      _zchunkLoader->cont().unwrap();
      return;
    } catch ( ... ) {
      MIL << "Continuing zchunk failed for " << _dl->url() << " trying to recover via normal download." << std::endl;
      normalDownload();
    }
    return;
  }
#endif

  // done, no error
  _lastError.reset();
  setFinished ();
}

void NetworkProvideItem::onAuthRequired( zyppng::NetworkRequest &, zyppng::NetworkAuthData &auth, const std::string &availAuth )
{
  _parent.itemAuthRequired( shared_this<NetworkProvideItem>(), auth, availAuth );
}

void NetworkProvideItem::normalDownload()
{
#ifdef ENABLE_ZCHUNK_COMPRESSION
  // zchunk loader must be inactive
  _zchunkLoader.reset();
#endif

  // make sure no old ranges are lingering around
  _dl->resetRequestRanges();

  // ready, go
  _parent._dlManager->enqueue(_dl);
}

#ifdef ENABLE_ZCHUNK_COMPRESSION
void NetworkProvideItem::onZckBlocksRequired(const std::vector<zyppng::ZckLoader::Block> &requiredBlocks)
{
  _dl->resetRequestRanges();

  for ( const auto &block : requiredBlocks ) {
    if ( block._checksum.size() && block._chksumtype.size() ) {
      std::optional<zypp::Digest> dig = zypp::Digest();
      if ( !dig->create( block._chksumtype ) ) {
        WAR << "Trying to create Digest with chksum type " << block._chksumtype << " failed " << std::endl;
        _zchunkLoader->setFailed( zypp::str::Str() << "Trying to create Digest with chksum type " << block._chksumtype << " failed " );
        return;
      }

      if ( zypp::env::ZYPP_MEDIA_CURL_DEBUG() > 3 )
        DBG << "Starting block " << block._start << " with checksum " << zypp::Digest::digestVectorToString( block._checksum ) << "." << std::endl;
      _dl->addRequestRange( block._start, block._len, std::move(dig), block._checksum, {}, block._relevantDigestLen, block._chksumPad );
    } else {
      if ( zypp::env::ZYPP_MEDIA_CURL_DEBUG() > 3 )
        DBG << "Starting block " << block._start << " without checksum!" << std::endl;
      _dl->addRequestRange( block._start, block._len );
    }
  };

  _parent._dlManager->enqueue(_dl);
}

void NetworkProvideItem::onZckFinished(zyppng::ZckLoader::PrepareResult result)
{
  switch(result._code) {
    case zyppng::ZckLoader::PrepareResult::Error: {
      ERR << "Failed to setup zchunk because of: " << result._message << std::endl;
      normalDownload();
      return;
    } case zyppng::ZckLoader::PrepareResult::ExceedsMaxLen: {
      _lastError = zyppng::NetworkRequestErrorPrivate::customError( zyppng::NetworkRequestError::ExceededMaxLen );
      setFinished ();
      return;

    } case zyppng::ZckLoader::PrepareResult::Success:
      case zyppng::ZckLoader::PrepareResult::NothingToDo: {
      // done, no error
      _lastError.reset();
      setFinished();
      return;
    }
  }
}
#endif

NetworkProvider::NetworkProvider( std::string_view workerName )
  : zyppng::worker::ProvideWorker( workerName )
  , _dlManager( std::make_shared<zyppng::NetworkRequestDispatcher>() )
{
  // we only want to hear about new provides
  setProvNotificationMode( ProvideWorker::ONLY_NEW_PROVIDES );
}

zyppng::expected<zyppng::worker::WorkerCaps> NetworkProvider::initialize( const zyppng::worker::Configuration &conf )
{
  const auto iEnd = conf.end();
  if ( const auto &i = conf.find( std::string(zyppng::AGENT_STRING_CONF) ); i != iEnd ) {
    const auto &val = i->second;
    MIL << "Setting agent string to: " << val << std::endl;
    _dlManager->setAgentString( val );
  }
  if ( const auto &i = conf.find( std::string(zyppng::DISTRO_FLAV_CONF) ); i != iEnd ) {
    const auto &val = i->second;
    MIL << "Setting distro flavor header to: " << val << std::endl;
    _dlManager->setHostSpecificHeader("download.opensuse.org", "X-ZYpp-DistributionFlavor", val );
  }
  if ( const auto &i = conf.find( std::string(zyppng::ANON_ID_CONF) ); i != iEnd ) {
    const auto &val = i->second;
    MIL << "Got anonymous ID setting from controller" << std::endl;
    _dlManager->setHostSpecificHeader("download.opensuse.org", "X-ZYpp-AnonymousId", val );
  }
  if ( const auto &i = conf.find( std::string(zyppng::ATTACH_POINT) ); i != iEnd ) {
    const auto &val = i->second;
    MIL << "Got attachpoint from controller: " << val << std::endl;
    _attachPoint = val;
  } else {
    return zyppng::expected<zyppng::worker::WorkerCaps>::error(ZYPP_EXCPT_PTR( zypp::Exception("Attach point required to work.") ));
  }

  zyppng::worker::WorkerCaps caps;
  caps.set_worker_type ( zyppng::worker::WorkerCaps::Downloading );
  caps.set_cfg_flags(
    zyppng::worker::WorkerCaps::Flags (
      zyppng::worker::WorkerCaps::Pipeline
      | zyppng::worker::WorkerCaps::ZyppLogFormat
      )
    );

  _dlManager->run();

  return zyppng::expected<zyppng::worker::WorkerCaps>::success(caps);
}

void NetworkProvider::provide()
{
  auto &queue = requestQueue();

  if ( !queue.size() )
    return;

  const auto now = std::chrono::steady_clock::now();
  auto &io = controlIO();

  while ( io.readFdOpen() ) {
    // find next pending item
    auto i = std::find_if( queue.begin(), queue.end(), [&now]( const zyppng::worker::ProvideWorkerItemRef &req ){
      NetworkProvideItemRef nReq = std::static_pointer_cast<NetworkProvideItem>(req);
      return nReq && nReq->_state == zyppng::worker::ProvideWorkerItem::Pending && nReq->_scheduleAfter < now;
    });

    // none left, bail out
    if ( i == queue.end () )
      break;

    auto req = std::static_pointer_cast<NetworkProvideItem>( *i );
    if ( req->_state != zyppng::worker::ProvideWorkerItem::Pending )
      return;

    MIL_PRV << "About to schedule: " << req->_spec.code() << " with ID: "<< req->_spec.requestId() << std::endl;

    // here we only receive request codes, we only support Provide messages, all others are rejected
    // Cancel is never to be received here
    if ( req->_spec.code() != zyppng::ProvideMessage::Code::Prov ) {
      req->_state = zyppng::worker::ProvideWorkerItem::Finished;
      provideFailed( req->_spec.requestId()
        , zyppng::ProvideMessage::Code::BadRequest
        , "Request type not implemented"
        , false
        , {} );

      queue.erase(i);
      continue;
    }

    zypp::Url url;
    const auto &urlVal = req->_spec.value( zyppng::ProvideMsgFields::Url );
    try {
      url = zypp::Url( urlVal.asString() );
    }  catch ( const zypp::Exception &excp ) {
      ZYPP_CAUGHT(excp);

      std::string err = zypp::str::Str() << "Invalid URL in request: " << urlVal.asString();
      ERR << err << std::endl;

      req->_state = zyppng::worker::ProvideWorkerItem::Finished;
      provideFailed( req->_spec.requestId()
        , zyppng::ProvideMessage::Code::BadRequest
        , err
        , false
        , {} );

      queue.erase(i);
      continue;
    }

    const auto &fPath = zypp::Pathname( url.getPathName() );

    /*
     * We download into the staging directory first, using the md5sum of the full path as filename.
     * Once the download is finished its transferred into the cache directory. This way we can clearly
     * distinguish between cancelled downloads and downloads we have completely finished.
     */
    zypp::Pathname localPath   = zypp::Pathname( _attachPoint ) / "cache" / fPath;
    zypp::Pathname stagingPath = ( zypp::Pathname( _attachPoint ) / "staging" / zypp::CheckSum::md5FromString( fPath.asString() ).asString() ).extend(".part");

    MIL_PRV << "Providing " << url << " to local: " << localPath << " staging: " << stagingPath << std::endl;

    // Cache hit?
    zypp::PathInfo cacheFile(localPath);
    if ( cacheFile.isExist () ) {

      // could be a directory
      if ( !cacheFile.isFile() ) {
        MIL_PRV << "Path " << url << " exists in Cache but its not a file!" << std::endl;
        req->_state = zyppng::worker::ProvideWorkerItem::Finished;
        provideFailed( req->_spec.requestId()
          , zyppng::ProvideMessage::Code::NotAFile
          , "Not a file"
          , false
          , {} );

        queue.erase(i);
        continue;
      }

      MIL_PRV << "Providing " << url << "Cache HIT" << std::endl;
      req->_state = zyppng::worker::ProvideWorkerItem::Finished;
      provideSuccess ( req->_spec.requestId(), true, localPath );
      queue.erase(i);
      continue;
    }

    if ( zypp::PathInfo(stagingPath).isExist () ) {
      // check if there is another request running with this staging file name
      auto runningIt = std::find_if( queue.begin (), queue.end(), [&]( const zyppng::worker::ProvideWorkerItemRef &queueItem ){
        if ( req == queueItem || queueItem->_state != zyppng::worker::ProvideWorkerItem::Running )
          return false;
        return ( static_cast<NetworkProvideItem*>( queueItem.get() )->_stagingFileName == stagingPath );
      });

      // we schedule that later again which should result in a immediate cache hit
      if ( runningIt != queue.end() ) {
        MIL << "Found a existing request that results in the same staging file, postponing the request for later" << std::endl;
        req->_scheduleAfter = now;
        continue;
      }

      MIL<< "Removing broken staging file" << stagingPath << std::endl;

      // here this is most likely a broken download
      zypp::filesystem::unlink( stagingPath );
    }

    auto errCode = zypp::filesystem::assert_dir( localPath.dirname() );
    if( errCode ) {
      std::string err = zypp::str::Str() << "assert_dir " << localPath.dirname() << " failed";
      DBG << err << std::endl;

      req->_state = zyppng::worker::ProvideWorkerItem::Finished;
      provideFailed( req->_spec.requestId()
        , zyppng::ProvideMessage::Code::InternalError
        , err
        , false
        , {} );
      queue.erase(i);
      continue;
    }

    errCode = zypp::filesystem::assert_dir( stagingPath.dirname() );
    if( errCode ) {
      std::string err = zypp::str::Str() << "assert_dir " << stagingPath.dirname() << " failed";
      DBG << err << std::endl;

      req->_state = zyppng::worker::ProvideWorkerItem::Finished;
      provideFailed( req->_spec.requestId()
        , zyppng::ProvideMessage::Code::InternalError
        , err
        , false
        , {} );
      queue.erase(i);
      continue;
    }

    req->_targetFileName  = localPath;
    req->_stagingFileName = stagingPath;

    req->startDownload( url );
  }
}

void NetworkProvider::cancel( const std::deque<zyppng::worker::ProvideWorkerItemRef>::iterator &i )
{
  auto &queue = requestQueue ();
  if ( i == queue.end() ) {
    ERR << "Unknown request ID, ignoring." << std::endl;
    return;
  }

  const auto &req = std::static_pointer_cast<NetworkProvideItem>(*i);
  req->cancelDownload();
  queue.erase(i);
}

void NetworkProvider::immediateShutdown()
{
  for ( const auto &pItem : requestQueue () ) {
    auto ref = std::static_pointer_cast<NetworkProvideItem>( pItem );
    ref->cancelDownload();
  }
}

zyppng::worker::ProvideWorkerItemRef NetworkProvider::makeItem(zyppng::ProvideMessage &&spec )
{
  return std::make_shared<NetworkProvideItem>( *this, std::move(spec) );
}

void NetworkProvider::itemStarted( NetworkProvideItemRef item )
{
  provideStart( item->_spec.requestId(), item->_dl->url().asCompleteString(), item->_targetFileName.asString(), item->_stagingFileName.asString() );
}

void NetworkProvider::itemFinished( NetworkProvideItemRef item )
{
  auto &queue = requestQueue ();
  auto i = std::find( queue.begin(), queue.end(), item );
  if ( i == queue.end() ) {
    ERR << "Unknown request finished. This is a BUG!" << std::endl;
    return;
  }

  item->_state = NetworkProvideItem::Finished;

  const auto &maybeError = item->error();
  if ( !maybeError ) {

    if ( item->_checkExistsOnly ) {
      // check exist only, we just return success with the target file name
      provideSuccess( item->_spec.requestId(), false, item->_targetFileName );
    }

    const auto errCode = zypp::filesystem::rename( item->_stagingFileName, item->_targetFileName );
    if( errCode ) {

      zypp::filesystem::unlink( item->_stagingFileName );

      std::string err = zypp::str::Str() << "Renaming " << item->_stagingFileName << " to " << item->_targetFileName << " failed!";
      DBG << err << std::endl;

      provideFailed( item->_spec.requestId()
        , zyppng::ProvideMessage::Code::InternalError
        , err
        , false
        , {} );

    } else {
      provideSuccess( item->_spec.requestId(), false, item->_targetFileName );
    }

  } else {
    zyppng::HeaderValueMap extra;
    bool isTransient = false;
    auto errCode = zyppng::ProvideMessage::Code::InternalError;

    const auto &error = maybeError.value();
    switch( error.type() ) {
      case zyppng::NetworkRequestError::NoError: { throw std::runtime_error("DownloadError info broken"); break;}
      case zyppng::NetworkRequestError::InternalError: { errCode = zyppng::ProvideMessage::Code::InternalError; break;}
      case zyppng::NetworkRequestError::RangeFail: { errCode = zyppng::ProvideMessage::Code::InternalError; break;}
      case zyppng::NetworkRequestError::Cancelled: { errCode = zyppng::ProvideMessage::Code::Cancelled; break;}
      case zyppng::NetworkRequestError::PeerCertificateInvalid: { errCode = zyppng::ProvideMessage::Code::PeerCertificateInvalid; break;}
      case zyppng::NetworkRequestError::ConnectionFailed: { errCode = zyppng::ProvideMessage::Code::ConnectionFailed; break;}
      case zyppng::NetworkRequestError::ExceededMaxLen: { errCode = zyppng::ProvideMessage::Code::ExpectedSizeExceeded; break;}
      case zyppng::NetworkRequestError::InvalidChecksum: { errCode = zyppng::ProvideMessage::Code::InvalidChecksum; break;}
      case zyppng::NetworkRequestError::UnsupportedProtocol: {  errCode = zyppng::ProvideMessage::Code::BadRequest; break; }
      case zyppng::NetworkRequestError::MalformedURL: { errCode = zyppng::ProvideMessage::Code::BadRequest; break; }
      case zyppng::NetworkRequestError::TemporaryProblem: { errCode = zyppng::ProvideMessage::Code::InternalError; isTransient = true; break;}
      case zyppng::NetworkRequestError::Timeout: { errCode = zyppng::ProvideMessage::Code::Timeout; break;}
      case zyppng::NetworkRequestError::Forbidden: { errCode = zyppng::ProvideMessage::Code::Forbidden; break;}
      case zyppng::NetworkRequestError::NotFound: { errCode = zyppng::ProvideMessage::Code::NotFound; break;}
      case zyppng::NetworkRequestError::Unauthorized: {
        errCode = zyppng::ProvideMessage::Code::Unauthorized;
        const auto &authHint = error.extraInfoValue("authHint", std::string());
        if ( authHint.size () )
          extra.set ( "authhint", authHint );
        break;
      }
      case zyppng::NetworkRequestError::AuthFailed: {
        errCode = zyppng::ProvideMessage::Code::NoAuthData;
        const auto &authHint = error.extraInfoValue("authHint", std::string());
        if ( authHint.size () )
          extra.set ( "authhint", authHint );
        break;
      }
      //@TODO add extra codes for handling these
      case zyppng::NetworkRequestError::Http2Error:
      case zyppng::NetworkRequestError::Http2StreamError:
      case zyppng::NetworkRequestError::ServerReturnedError: { errCode = zyppng::ProvideMessage::Code::InternalError; break; }
      case zyppng::NetworkRequestError::MissingData: { errCode = zyppng::ProvideMessage::Code::BadRequest; break; }
    }

    provideFailed( item->_spec.requestId(), errCode, error.toString(), isTransient, extra );
  }
  queue.erase(i);

  // pick the next request
  provide();
}

void NetworkProvider::itemAuthRequired( NetworkProvideItemRef item, zyppng::NetworkAuthData &auth, const std::string & availAuth )
{
  auto res = requireAuthorization( item->_spec.requestId(), auth.url(), auth.username(), auth.lastDatabaseUpdate(), { { std::string( zyppng::AuthDataRequestMsgFields::AuthHint ), availAuth } } );
  if ( !res ) {
    auth = zyppng::NetworkAuthData();
    return;
  }

  auth.setUrl ( item->_dl->url() );
  auth.setUsername ( res->username );
  auth.setPassword ( res->password );
  auth.setLastDatabaseUpdate ( res->last_auth_timestamp );
  auth.extraValues() = res->extraKeys;

  const auto &keyStr = std::string(zyppng::AuthInfoMsgFields::AuthType);
  if ( res->extraKeys.count( keyStr ) ) {
    auth.setAuthType( res->extraKeys[keyStr] );
  }

  //@todo migrate away from using NetworkAuthData, AuthData now has a map of extra values that can carry the AuthType
  const auto &authTypeKey = std::string( zyppng::AuthInfoMsgFields::AuthType );
  if ( res->extraKeys.count( authTypeKey ) )
    auth.setAuthType( res->extraKeys[ authTypeKey ] );
}
