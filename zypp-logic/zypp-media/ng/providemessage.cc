/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "private/providemessage_p.h"
#include "zypp-core/zyppng/rpc/stompframestream.h"

#include <zypp-core/Url.h>
#include <string_view>
#include <string>

namespace zyppng {

  zyppng::expected<zypp::PluginFrame> ProviderConfiguration::toStompMessage() const
  {
    auto frame = rpc::prepareFrame<ProviderConfiguration>();
    for ( const auto &v : *this ) {
      frame.addHeader( v.first, v.second );
    }
    return zyppng::expected<zypp::PluginFrame>::success( std::move(frame) );
  }

  zyppng::expected<ProviderConfiguration> ProviderConfiguration::fromStompMessage(const zypp::PluginFrame &msg)
  {
    try {
      if ( msg.command() != ProviderConfiguration::typeName )
        ZYPP_THROW( InvalidMessageReceivedException("Message is not of type ProviderConfiguration") );

      ProviderConfiguration res;
      for ( auto i = msg.headerBegin (); i != msg.headerEnd(); i++  ) {
        res.insert_or_assign ( i->first, i->second );
      }

      return zyppng::expected<ProviderConfiguration>::success ( std::move(res) );

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<ProviderConfiguration>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

  /*!
   * Worker Capabilities , sent by the workers to the provider
   */
  WorkerCaps::WorkerCaps()
  { }

  WorkerCaps::~WorkerCaps()
  { }

  uint32_t WorkerCaps::protocol_version() const
  {
    return _protocolVersion;
  }

  WorkerCaps::WorkerType WorkerCaps::worker_type() const
  {
    return static_cast<WorkerType>(_workerType);
  }

  WorkerCaps::Flags WorkerCaps::cfg_flags() const
  {
    return static_cast<WorkerCaps::Flags>(_cfgFlags);
  }

  const std::string &WorkerCaps::worker_name() const
  {
    return _workerName;
  }

  void WorkerCaps::set_protocol_version(uint32_t v)
  {
    _protocolVersion = v;
  }

  void WorkerCaps::set_worker_type(WorkerType t)
  {
    _workerType = t;
  }

  void WorkerCaps::set_cfg_flags(Flags f)
  {
    _cfgFlags = f;
  }

  void WorkerCaps::set_worker_name(std::string name)
  {
    _workerName = std::move(name);
  }

  zyppng::expected<zypp::PluginFrame> WorkerCaps::toStompMessage() const
  {
    try {
      zypp::PluginFrame pf = zyppng::rpc::prepareFrame<WorkerCaps>();
      pf.addHeader ( "protocolVersion", zypp::str::asString (_protocolVersion) );
      pf.addHeader ( "workerType"     , zypp::str::asString (_workerType) );
      pf.addHeader ( "cfgFlags"       , zypp::str::asString (_cfgFlags) );
      pf.addHeader ( "workerName"     , zypp::str::asString (_workerName) );
      return zyppng::expected<zypp::PluginFrame>::success ( std::move(pf) );

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<zypp::PluginFrame>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

  zyppng::expected<WorkerCaps> WorkerCaps::fromStompMessage(const zypp::PluginFrame &msg)
  {
    try {
      if ( msg.command() != WorkerCaps::typeName )
        ZYPP_THROW( InvalidMessageReceivedException("Message is not of type WorkerCaps") );

      WorkerCaps res;
      zyppng::rpc::parseHeaderIntoField( msg, "protocolVersion", res._protocolVersion );
      zyppng::rpc::parseHeaderIntoField( msg, "workerType", res._workerType );
      zyppng::rpc::parseHeaderIntoField( msg, "cfgFlags", res._cfgFlags );
      zyppng::rpc::parseHeaderIntoField( msg, "workerName", res._workerName );
      return zyppng::expected<WorkerCaps>::success ( std::move(res) );

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT (e);
      return zyppng::expected<WorkerCaps>::error( ZYPP_EXCPT_PTR(e) );
    }
  }

  template <typename T>
  static zyppng::expected<void> doParseField( const std::string &val, ProvideMessage &t, std::string_view msgtype, std::string_view name ) {
    try {
      T tVal; // the target value type
      zyppng::rpc::parseDataIntoField ( val, tVal );
      t.addValue( name, std::move(tVal) );
      return zyppng::expected<void>::success();
    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT(e);
      return zyppng::expected<void>::error( ZYPP_EXCPT_PTR( InvalidMessageReceivedException( zypp::str::Str() << "Parse error " << msgtype << ", Field " << name << " has invalid type" ) ) );
    }
  }

  ProvideMessage::ProvideMessage()
  { }

  expected<zyppng::ProvideMessage> ProvideMessage::create(const zypp::PluginFrame &msg)
  {
    if ( msg.command() != ProvideMessage::typeName ) {
      return zyppng::expected<ProvideMessage>::error( ZYPP_EXCPT_PTR( InvalidMessageReceivedException("Message is not of type WorkerCaps") ) );
    }

    const std::string &codeStr = msg.getHeaderNT( std::string(ProvideMessageFields::RequestCode) );
    if ( codeStr.empty () ) {
      return zyppng::expected<ProvideMessage>::error( ZYPP_EXCPT_PTR ( InvalidMessageReceivedException("Invalid message, PluginFrame has no requestId header.")) );
    }

    const auto c = zyppng::str::safe_strtonum<uint32_t>( codeStr ).value_or ( NoCode );
    const auto validCode =    ( c >= ProvideMessage::Code::FirstInformalCode    && c <= ProvideMessage::Code::LastInformalCode  )
                           || ( c >= ProvideMessage::Code::FirstSuccessCode     && c <= ProvideMessage::Code::LastSuccessCode   )
                           || ( c >= ProvideMessage::Code::FirstRedirCode       && c <= ProvideMessage::Code::LastRedirCode     )
                           || ( c >= ProvideMessage::Code::FirstClientErrCode   && c <= ProvideMessage::Code::LastClientErrCode )
                           || ( c >= ProvideMessage::Code::FirstSrvErrCode      && c <= ProvideMessage::Code::LastSrvErrCode    )
                           || ( c >= ProvideMessage::Code::FirstControllerCode  && c <= ProvideMessage::Code::LastControllerCode)
                           || ( c >= ProvideMessage::Code::FirstWorkerCode      && c <= ProvideMessage::Code::LastWorkerCode    );
    if ( !validCode ) {
      return zyppng::expected<ProvideMessage>::error( ZYPP_EXCPT_PTR ( InvalidMessageReceivedException("Invalid code in PluginFrame")) );
    }

    const std::string & idStr = msg.getHeaderNT( std::string(ProvideMessageFields::RequestId) );
    if ( idStr.empty () ) {
      return zyppng::expected<ProvideMessage>::error( ZYPP_EXCPT_PTR ( InvalidMessageReceivedException("Invalid message, PluginFrame has no requestId header.")) );
    }

    const auto &maybeId = zyppng::str::safe_strtonum<uint>( idStr );
    if ( !maybeId ) {
      return zyppng::expected<ProvideMessage>::error( ZYPP_EXCPT_PTR ( InvalidMessageReceivedException("Invalid message, can not parse requestId header.")) );
    }

    ProvideMessage pMessage;
    pMessage.setCode ( static_cast<MessageCodes>(c) );
    pMessage.setRequestId ( *maybeId );

    #define DEF_REQ_FIELD( fname ) bool has_##fname = false

    #define PARSE_FIELD( msgtype, fname, ftype, _C_ ) \
      if ( name == #fname ) { \
        const auto &res = doParseField<ftype>( val, pMessage, #msgtype, #fname ); \
        if ( !res ) { \
          return zyppng::expected<ProvideMessage>::error(res.error()); \
        } \
        _C_ \
      }

    #define HANDLE_UNKNOWN_FIELD( fname, val ) { \
        pMessage.addValue( fname, val ); \
      }

    #define OR_HANDLE_UNKNOWN_FIELD( fname, val ) else HANDLE_UNKNOWN_FIELD( fname, val )

    #define BEGIN_PARSE_HEADERS \
    for ( const auto &header : msg.headerList() ) { \
      const auto &name = header.first;  \
      const auto &val  = header.second;

    #define END_PARSE_HEADERS }

    #define PARSE_REQ_FIELD( msgtype, fname, ftype ) PARSE_FIELD( msgtype, fname, ftype, has_##fname = true; )
    #define OR_PARSE_REQ_FIELD( msgtype, fname, ftype ) else PARSE_REQ_FIELD( msgtype, fname, ftype )
    #define PARSE_OPT_FIELD( msgtype, fname, ftype ) PARSE_FIELD( msgtype, fname, ftype, )
    #define OR_PARSE_OPT_FIELD( msgtype, fname, ftype ) else PARSE_OPT_FIELD( msgtype, fname, ftype )

    #define FAIL_IF_NOT_SEEN_REQ_FIELD( msgtype, fname ) \
      if ( !has_##fname ) \
        return expected<ProvideMessage>::error( ZYPP_EXCPT_PTR( InvalidMessageReceivedException( zypp::str::Str() << #msgtype <<" message does not contain required " << #fname << " field" ) ) )

    auto validateErrorMsg = [&]( const auto &msg ){
      DEF_REQ_FIELD(reason);
      BEGIN_PARSE_HEADERS
        PARSE_REQ_FIELD ( Error, reason, std::string )
        OR_PARSE_OPT_FIELD ( Error, history, std::string )
        OR_PARSE_OPT_FIELD ( Error, transient, bool )
        OR_HANDLE_UNKNOWN_FIELD( name, val )
      END_PARSE_HEADERS
      FAIL_IF_NOT_SEEN_REQ_FIELD( Error, reason );
      return expected<ProvideMessage>::success( std::move(pMessage) );
    };

    switch ( c )
    {
      case ProvideMessage::Code::ProvideStarted: {
        DEF_REQ_FIELD(url);
        BEGIN_PARSE_HEADERS
          PARSE_REQ_FIELD     ( ProvideStarted, url, std::string )
          OR_PARSE_OPT_FIELD  ( ProvideStarted, local_filename, std::string )
          OR_PARSE_OPT_FIELD  ( ProvideStarted, staging_filename, std::string )
          OR_HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        FAIL_IF_NOT_SEEN_REQ_FIELD( ProvideStarted, url );
        return expected<ProvideMessage>::success( std::move(pMessage) );
      }
      case ProvideMessage::Code::ProvideFinished: {
        DEF_REQ_FIELD(cacheHit);
        DEF_REQ_FIELD(local_filename);
        BEGIN_PARSE_HEADERS
          PARSE_REQ_FIELD     ( ProvideFinished, cacheHit,       bool )
          OR_PARSE_REQ_FIELD  ( ProvideFinished, local_filename, std::string )
          OR_HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        FAIL_IF_NOT_SEEN_REQ_FIELD( ProvideFinished, cacheHit );
        FAIL_IF_NOT_SEEN_REQ_FIELD( ProvideFinished, local_filename );
        return expected<ProvideMessage>::success( std::move(pMessage) );
      }
      case ProvideMessage::Code::AttachFinished: {
        BEGIN_PARSE_HEADERS
          PARSE_OPT_FIELD ( AttachFinished, local_mountpoint, std::string )
          OR_HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        return expected<ProvideMessage>::success( std::move(pMessage) );
      }
      case ProvideMessage::Code::DetachFinished: {
        // no known fields
        BEGIN_PARSE_HEADERS
          HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        return expected<ProvideMessage>::success( std::move(pMessage) );
      }
      case ProvideMessage::Code::AuthInfo: {
        DEF_REQ_FIELD(username);
        DEF_REQ_FIELD(password);
        DEF_REQ_FIELD(auth_timestamp);
        BEGIN_PARSE_HEADERS
          PARSE_REQ_FIELD    ( AuthInfo, username, std::string )
          OR_PARSE_REQ_FIELD ( AuthInfo, password, std::string )
          OR_PARSE_REQ_FIELD ( AuthInfo, auth_timestamp, int64_t )
          OR_PARSE_OPT_FIELD ( AuthInfo, authType, std::string )
          OR_HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        FAIL_IF_NOT_SEEN_REQ_FIELD( ProvideStarted, username );
        FAIL_IF_NOT_SEEN_REQ_FIELD( ProvideStarted, password );
        FAIL_IF_NOT_SEEN_REQ_FIELD( ProvideStarted, auth_timestamp );
        return expected<ProvideMessage>::success( std::move(pMessage) );
      }
      case ProvideMessage::Code::MediaChanged:
        // no known fields
        BEGIN_PARSE_HEADERS
          HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        return expected<ProvideMessage>::success( std::move(pMessage) );

      case ProvideMessage::Code::Redirect: {
        DEF_REQ_FIELD(new_url);
        BEGIN_PARSE_HEADERS
          PARSE_REQ_FIELD ( Redirect, new_url, std::string )
          OR_HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        FAIL_IF_NOT_SEEN_REQ_FIELD( Redirect, new_url );
        return expected<ProvideMessage>::success( std::move(pMessage) );
      }
      case ProvideMessage::Code::Metalink: {
        DEF_REQ_FIELD(new_url);
        BEGIN_PARSE_HEADERS
          PARSE_REQ_FIELD ( Metalink, new_url, std::string )
          OR_HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        FAIL_IF_NOT_SEEN_REQ_FIELD( Metalink, new_url );
        return expected<ProvideMessage>::success( std::move(pMessage) );
      }
      case ProvideMessage::Code::BadRequest:
      case ProvideMessage::Code::Unauthorized:
      case ProvideMessage::Code::Forbidden:
      case ProvideMessage::Code::PeerCertificateInvalid:
      case ProvideMessage::Code::NotFound:
      case ProvideMessage::Code::ExpectedSizeExceeded:
      case ProvideMessage::Code::ConnectionFailed:
      case ProvideMessage::Code::Timeout:
      case ProvideMessage::Code::Cancelled:
      case ProvideMessage::Code::InvalidChecksum:
      case ProvideMessage::Code::MountFailed:
      case ProvideMessage::Code::Jammed:
      case ProvideMessage::Code::NoAuthData:
      case ProvideMessage::Code::MediaChangeAbort:
      case ProvideMessage::Code::MediaChangeSkip:
      case ProvideMessage::Code::InternalError: {
        return validateErrorMsg(msg);
      }
      case ProvideMessage::Code::Prov: {
        DEF_REQ_FIELD(url);
        BEGIN_PARSE_HEADERS
          PARSE_REQ_FIELD    ( Provide, url, std::string )
          OR_PARSE_OPT_FIELD ( Provide, filename, std::string )
          OR_PARSE_OPT_FIELD ( Provide, delta_file, std::string )
          OR_PARSE_OPT_FIELD ( Provide, expected_filesize, int64_t )
          OR_PARSE_OPT_FIELD ( Provide, check_existance_only, bool )
          OR_PARSE_OPT_FIELD ( Provide, metalink_enabled, bool )
          OR_HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        FAIL_IF_NOT_SEEN_REQ_FIELD( Provide, url );
        return expected<ProvideMessage>::success( std::move(pMessage) );
      }
      case ProvideMessage::Code::Cancel:
        // no known fields
        BEGIN_PARSE_HEADERS
          HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        return expected<ProvideMessage>::success( std::move(pMessage) );

      case ProvideMessage::Code::Attach: {
        std::exception_ptr error;

        DEF_REQ_FIELD(url);
        DEF_REQ_FIELD(attach_id);
        DEF_REQ_FIELD(label);

        // not really required, but this way we can check if all false or all true
        DEF_REQ_FIELD(verify_type);
        DEF_REQ_FIELD(verify_data);
        DEF_REQ_FIELD(media_nr);

        BEGIN_PARSE_HEADERS
          PARSE_REQ_FIELD    ( Attach, url        , std::string )
          OR_PARSE_REQ_FIELD ( Attach, attach_id  , std::string )
          OR_PARSE_REQ_FIELD ( Attach, label      , std::string )
          OR_PARSE_REQ_FIELD ( Attach, verify_type, std::string )
          OR_PARSE_REQ_FIELD ( Attach, verify_data, std::string )
          OR_PARSE_REQ_FIELD ( Attach, media_nr   , int32_t )
          OR_PARSE_OPT_FIELD ( Attach, device     , std::string )
          OR_HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        FAIL_IF_NOT_SEEN_REQ_FIELD( Provide, url );
        FAIL_IF_NOT_SEEN_REQ_FIELD( Provide, label );
        FAIL_IF_NOT_SEEN_REQ_FIELD( Provide, attach_id );
        if ( ! ( ( has_verify_data == has_verify_type ) && ( has_verify_type == has_media_nr ) ) )
          return expected<ProvideMessage>::error( ZYPP_EXCPT_PTR ( InvalidMessageReceivedException("Error in Attach message, one of the following fields is not set or invalid: ( verify_type, verify_data, media_nr ). Either none or all need to be set. ")) );

        return expected<ProvideMessage>::success( std::move(pMessage) );
      }
      case ProvideMessage::Code::Detach: {
        DEF_REQ_FIELD(url);
        BEGIN_PARSE_HEADERS
          PARSE_REQ_FIELD ( Detach, url, std::string )
          OR_HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        FAIL_IF_NOT_SEEN_REQ_FIELD( Detach, url );

        return expected<ProvideMessage>::success( std::move(pMessage) );
      }
      case ProvideMessage::Code::AuthDataRequest: {
        DEF_REQ_FIELD(effective_url);
        BEGIN_PARSE_HEADERS
          PARSE_REQ_FIELD     ( AuthDataRequest, effective_url,       std::string )
          OR_PARSE_OPT_FIELD  ( AuthDataRequest, last_auth_timestamp, int64_t     )
          OR_PARSE_OPT_FIELD  ( AuthDataRequest, username,            std::string )
          OR_PARSE_OPT_FIELD  ( AuthDataRequest, authHint,            std::string )
          OR_HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        FAIL_IF_NOT_SEEN_REQ_FIELD( AuthDataRequest, effective_url );

        return expected<ProvideMessage>::success( std::move(pMessage) );
      }
      case ProvideMessage::Code::MediaChangeRequest: {
        DEF_REQ_FIELD(label);
        DEF_REQ_FIELD(media_nr);
        DEF_REQ_FIELD(device);
        BEGIN_PARSE_HEADERS
          PARSE_REQ_FIELD    ( MediaChangeRequest, label,     std::string )
          OR_PARSE_REQ_FIELD ( MediaChangeRequest, media_nr,  int32_t     )
          OR_PARSE_REQ_FIELD ( MediaChangeRequest, device,    std::string )
          OR_PARSE_OPT_FIELD ( MediaChangeRequest, desc,      std::string )
          OR_HANDLE_UNKNOWN_FIELD( name, val )
        END_PARSE_HEADERS
        FAIL_IF_NOT_SEEN_REQ_FIELD( MediaChangeRequest, label );
        FAIL_IF_NOT_SEEN_REQ_FIELD( MediaChangeRequest, media_nr );
        FAIL_IF_NOT_SEEN_REQ_FIELD( MediaChangeRequest, device );

        return expected<ProvideMessage>::success( std::move(pMessage) );
      }
      default: {
        // all error messages have the same format
        if ( c >= ProvideMessage::Code::FirstClientErrCode && c <= ProvideMessage::Code::LastSrvErrCode ) {
          return validateErrorMsg(msg);
        }
      }
    }

    // we should never reach this line because we check in the beginning if the know the message code
    return zyppng::expected<ProvideMessage>::error( ZYPP_EXCPT_PTR ( InvalidMessageReceivedException("Unknown code in PluginFrame")) );
  }

  expected<zypp::PluginFrame> ProvideMessage::toStompMessage() const
  {
    zypp::PluginFrame f = rpc::prepareFrame<ProvideMessage>();
    f.addHeader ( std::string(ProvideMessageFields::RequestCode), zypp::str::asString (static_cast<uint32_t>(_code)) );
    f.addHeader ( std::string(ProvideMessageFields::RequestId), zypp::str::asString (_reqId) );
    for ( auto i = _headers.beginList (); i != _headers.endList(); i++ ) {
      for ( const auto &val : i->second ) {
        const auto &strVal = std::visit([&]( const auto &val ){
          if constexpr( std::is_same_v<std::decay_t<decltype(val)>, std::monostate> )
              return std::string();
          else {
            return zypp::str::asString(val);
          }
        }, val.asVariant() );
        if ( strVal.empty () )
          continue;
        f.addHeader( i->first, strVal );
      }
    }

    return expected<zypp::PluginFrame>::success ( std::move(f) );
  }

  expected<ProvideMessage> ProvideMessage::fromStompMessage(const zypp::PluginFrame &msg)
  {
    return ProvideMessage::create(msg);
  }

  ProvideMessage ProvideMessage::createProvideStarted( const uint32_t reqId, const zypp::Url &url, const std::optional<std::string> &localFilename, const std::optional<std::string> &stagingFilename )
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::ProvideStarted );
    msg.setRequestId ( reqId );
    msg.setValue ( ProvideStartedMsgFields::Url, url.asCompleteString() );
    if ( localFilename )
      msg.setValue ( ProvideStartedMsgFields::LocalFilename, *localFilename );
    if ( stagingFilename )
      msg.setValue ( ProvideStartedMsgFields::StagingFilename, *stagingFilename );

    return msg;
  }

  ProvideMessage ProvideMessage::createProvideFinished( const uint32_t reqId, const std::string &localFilename, bool cacheHit )
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::ProvideFinished );
    msg.setRequestId ( reqId );
    msg.setValue ( ProvideFinishedMsgFields::LocalFilename, localFilename );
    msg.setValue ( ProvideFinishedMsgFields::CacheHit, cacheHit );

    return msg;
  }

  ProvideMessage ProvideMessage::createAttachFinished(const uint32_t reqId , const std::optional<std::string> &localMountPoint )
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::AttachFinished );
    msg.setRequestId ( reqId );

    if ( localMountPoint )
      msg.setValue ( AttachFinishedMsgFields::LocalMountPoint, *localMountPoint );

    return msg;
  }

  ProvideMessage ProvideMessage::createDetachFinished(const uint32_t reqId)
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::DetachFinished );
    msg.setRequestId ( reqId );

    return msg;
  }

  ProvideMessage ProvideMessage::createAuthInfo( const uint32_t reqId, const std::string &user, const std::string &pw, int64_t timestamp, const std::map<std::string, std::string> &extraValues )
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::AuthInfo );
    msg.setRequestId ( reqId );
    msg.setValue ( AuthInfoMsgFields::Username, user  );
    msg.setValue ( AuthInfoMsgFields::Password, pw    );
    msg.setValue ( AuthInfoMsgFields::AuthTimestamp, timestamp );
    for ( const auto& i : extraValues ) {
      msg.setValue( i.first, i.second );
    }
    return msg;
  }

  ProvideMessage ProvideMessage::createMediaChanged( const uint32_t reqId )
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::MediaChanged );
    msg.setRequestId ( reqId );

    return msg;
  }

  ProvideMessage ProvideMessage::createRedirect( const uint32_t reqId, const zypp::Url &newUrl )
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::Redirect );
    msg.setRequestId ( reqId );
    msg.setValue ( RedirectMsgFields::NewUrl, newUrl.asCompleteString() );

    return msg;
  }

  ProvideMessage ProvideMessage::createMetalinkRedir( const uint32_t reqId, const std::vector<zypp::Url> &newUrls )
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::Metalink );
    msg.setRequestId ( reqId );
    for( const auto &val : newUrls )
      msg.addValue( MetalinkRedirectMsgFields::NewUrl, val.asCompleteString() );

    return msg;
  }

  ProvideMessage ProvideMessage::createErrorResponse( const uint32_t reqId, const Code code, const std::string &reason, bool transient )
  {
    ProvideMessage msg;
    if ( code < Code::FirstClientErrCode || code > Code::LastSrvErrCode )
      ZYPP_THROW(std::out_of_range("code must be between 400 and 599"));
    msg.setCode ( code );
    msg.setRequestId ( reqId );
    msg.setValue ( ErrMsgFields::Reason, reason );
    msg.setValue ( ErrMsgFields::Transient, transient );
    return msg;
  }

  ProvideMessage ProvideMessage::createProvide( const uint32_t reqId, const zypp::Url &url, const std::optional<std::string> &filename, const std::optional<std::string> &deltaFile, const std::optional<int64_t> &expFilesize, bool checkExistOnly )
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::Prov );
    msg.setRequestId ( reqId );
    msg.setValue ( ProvideMsgFields::Url, url.asCompleteString() );

    if ( filename )
      msg.setValue ( ProvideMsgFields::Filename, *filename );
    if ( deltaFile )
      msg.setValue ( ProvideMsgFields::DeltaFile, *deltaFile );
    if ( expFilesize )
      msg.setValue ( ProvideMsgFields::ExpectedFilesize, *expFilesize );
    msg.setValue ( ProvideMsgFields::CheckExistOnly, checkExistOnly );

    return msg;
  }

  ProvideMessage ProvideMessage::createCancel( const uint32_t reqId )
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::Cancel );
    msg.setRequestId ( reqId );

    return msg;
  }

  ProvideMessage ProvideMessage::createAttach(const uint32_t reqId, const zypp::Url &url, const std::string attachId, const std::string &label, const std::optional<std::string> &verifyType, const std::optional<std::string> &verifyData, const std::optional<int32_t> &mediaNr )
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::Attach );
    msg.setRequestId ( reqId );
    msg.setValue ( AttachMsgFields::Url, url.asCompleteString() );
    msg.setValue ( AttachMsgFields::AttachId, attachId );
    msg.setValue ( AttachMsgFields::Label, label );

    if ( verifyType.has_value() && verifyData.has_value() && mediaNr.has_value() ) {
      msg.setValue ( AttachMsgFields::VerifyType, *verifyType );
      msg.setValue ( AttachMsgFields::VerifyData, *verifyData );
      msg.setValue ( AttachMsgFields::MediaNr, *mediaNr );
    } else {
      if ( !( ( verifyType.has_value() == verifyData.has_value() ) && ( verifyData.has_value() == mediaNr.has_value() ) ) )
        WAR << "Attach message requires verifyType, verifyData and mediaNr either set together or not set at all." << std::endl;
    }

    return msg;
  }

  ProvideMessage ProvideMessage::createDetach( const uint32_t reqId, const zypp::Url &attachUrl )
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::Detach );
    msg.setRequestId ( reqId );
    msg.setValue ( DetachMsgFields::Url, attachUrl.asCompleteString() );

    return msg;
  }

  ProvideMessage ProvideMessage::createAuthDataRequest( const uint32_t reqId, const zypp::Url &effectiveUrl, const std::string &lastTriedUser, const std::optional<int64_t> &lastAuthTimestamp, const std::map<std::string, std::string> &extraValues )
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::AuthDataRequest );
    msg.setRequestId ( reqId );
    msg.setValue ( AuthDataRequestMsgFields::EffectiveUrl, effectiveUrl.asCompleteString() );
    if ( lastTriedUser.size() )
      msg.setValue( AuthDataRequestMsgFields::LastUser, lastTriedUser );
    if ( lastAuthTimestamp )
      msg.setValue ( AuthDataRequestMsgFields::LastAuthTimestamp, *lastAuthTimestamp );

    return msg;
  }

  ProvideMessage ProvideMessage::createMediaChangeRequest( const uint32_t reqId, const std::string &label, int32_t mediaNr, const std::vector<std::string> &devices, const std::optional<std::string> &desc )
  {
    ProvideMessage msg;
    msg.setCode ( ProvideMessage::Code::MediaChangeRequest );
    msg.setRequestId ( reqId );
    msg.setValue ( MediaChangeRequestMsgFields::Label, label );
    msg.setValue ( MediaChangeRequestMsgFields::MediaNr, mediaNr );
    for ( const auto &device : devices )
      msg.addValue ( MediaChangeRequestMsgFields::Device, device );
    if ( desc )
      msg.setValue ( MediaChangeRequestMsgFields::Desc, *desc );

    return msg;
  }

  uint ProvideMessage::requestId() const
  {
    return _reqId;
  }

  void ProvideMessage::setRequestId(const uint id)
  {
    _reqId = id;
  }

  ProvideMessage::Code ProvideMessage::code() const
  {
    return _code;
  }

  void ProvideMessage::setCode(const ProvideMessage::Code newCode )
  {
    _code = newCode;
  }

  const std::vector<ProvideMessage::FieldVal> &ProvideMessage::values( const std::string_view &str ) const
  {
    return _headers.values ( str );
  }

  const std::vector<ProvideMessage::FieldVal> &ProvideMessage::values( const std::string &str ) const
  {
    return values( std::string_view(str));
  }

  ProvideMessage::FieldVal ProvideMessage::value( const std::string_view &str, const FieldVal &defaultVal ) const
  {
    return _headers.value ( str, defaultVal );
  }

  const HeaderValueMap &ProvideMessage::headers() const
  {
    return _headers;
  }

  ProvideMessage::FieldVal ProvideMessage::value( const std::string &str, const FieldVal &defaultVal ) const
  {
    return value( std::string_view(str), defaultVal );
  }

  void ProvideMessage::setValue( const std::string &name, const FieldVal &value )
  {
    setValue( std::string_view(name), value );
  }

  void ProvideMessage::setValue( const std::string_view &name, const FieldVal &value )
  {
    _headers.set( std::string(name), value );
  }

  void ProvideMessage::addValue( const std::string &name, const FieldVal &value )
  {
    return addValue( std::string_view(name), value );
  }

  void ProvideMessage::addValue( const std::string_view &name, const FieldVal &value )
  {
    _headers.add( std::string(name), value );
  }

}
