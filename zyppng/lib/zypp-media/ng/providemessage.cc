/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "private/providemessage_p.h"
#include <zypp-core/ng/rpc/stompframestream.h>
#include <zypp-core/Url.h>
#include <algorithm>
#include <array>
#include <bitset>
#include <string_view>
#include <string>
#include <span>

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

  // --- Modernized Protocol Engine Implementation ---

  namespace {

    // FNV-1a 64-bit constants
    constexpr uint64_t FNV_OFFSET_BASIS = 0xcbf29ce484222325ULL;
    constexpr uint64_t FNV_PRIME        = 0x100000001b3ULL;

    /**
     * FNV-1a 64-bit hash function.
     * Guaranteed to be identical at compile-time and runtime.
     */
    constexpr uint64_t fnv1a(std::string_view s) {
      uint64_t hash = FNV_OFFSET_BASIS;
      for (size_t i = 0; i < s.size(); ++i) {
        hash ^= static_cast<uint64_t>(static_cast<uint8_t>(s[i]));
        hash *= FNV_PRIME;
      }
      return hash;
    }

    enum FieldRule { Required, Optional };

    struct FieldDef {
      uint64_t hash;
      std::string_view name;
      FieldRule rule;
      expected<void> (*parser)(const std::string&, ProvideMessage&, std::string_view, std::string_view);
    };

    /**
     * Non-templated view of a schema used by the population engine.
     */
    struct MessageSchema {
      std::string_view typeName;
      std::span<const FieldDef> fields;
      expected<void> (*customValidate)(const ProvideMessage&) = nullptr;
    };

    /**
     * Templated storage for sorted schemas.
     * The consteval constructor ensures sorting and collision checks happen strictly at compile-time.
     */
    template<size_t N>
    struct SortedSchema {
      std::string_view typeName;
      std::array<FieldDef, N> fields;
      expected<void> (*customValidate)(const ProvideMessage&) = nullptr;

      consteval SortedSchema(std::string_view name, std::array<FieldDef, N> inputFields, expected<void> (*cv)(const ProvideMessage&) = nullptr)
        : typeName(name), fields(inputFields), customValidate(cv)
      {
        std::sort(fields.begin(), fields.end(), [](const FieldDef& a, const FieldDef& b) {
          return a.hash < b.hash;
        });
        for (size_t i = 1; i < N; ++i) {
          if (fields[i].hash == fields[i-1].hash) throw "Hash collision in schema!";
        }
      }

      constexpr operator MessageSchema() const {
        return { typeName, fields, customValidate };
      }
    };

    template <typename T>
    static expected<void> doParseField( const std::string &val, ProvideMessage &t, std::string_view msgtype, std::string_view name ) {
      try {
        T tVal{};
        zyppng::rpc::parseDataIntoField ( val, tVal );
        t.addValue( name, std::move(tVal) );
        return expected<void>::success();
      } catch ( const zypp::Exception &e ) {
        return expected<void>::error( ZYPP_EXCPT_PTR( InvalidMessageReceivedException(
          zypp::str::Str() << "Parse error " << msgtype << ", Field " << name << " has invalid format: " << val ) ) );
      }
    }

    /**
     * Custom validation logic for Attach messages.
     */
    static expected<void> validateAttachVerification(const ProvideMessage& msg) {
      const auto & h = msg.headers();
      bool hasType = h.contains(AttachMsgFields::VerifyType);
      bool hasData = h.contains(AttachMsgFields::VerifyData);
      bool hasNr   = h.contains(AttachMsgFields::MediaNr);

      if (!((hasType == hasData) && (hasData == hasNr))) {
        return expected<void>::error( ZYPP_EXCPT_PTR ( InvalidMessageReceivedException(
          "Error in Attach message: verify_type, verify_data, and media_nr must all be set together or not at all." )) );
      }
      return expected<void>::success();
    }

    #define DEF_FIELD(name, rule, type) FieldDef{ fnv1a(name), name, rule, &doParseField<type> }

    // --- Message Schemas ---

    static constexpr auto ProvideStartedSchema = SortedSchema("ProvideStarted", std::to_array<FieldDef>({
      DEF_FIELD(ProvideStartedMsgFields::Url,             Required, std::string),
      DEF_FIELD(ProvideStartedMsgFields::LocalFilename,   Optional, std::string),
      DEF_FIELD(ProvideStartedMsgFields::StagingFilename, Optional, std::string)
    }));

    static constexpr auto ProvideFinishedSchema = SortedSchema("ProvideFinished", std::to_array<FieldDef>({
      DEF_FIELD(ProvideFinishedMsgFields::CacheHit,       Required, bool),
      DEF_FIELD(ProvideFinishedMsgFields::LocalFilename,  Required, std::string)
    }));

    static constexpr auto AttachFinishedSchema = SortedSchema("AttachFinished", std::to_array<FieldDef>({
      DEF_FIELD(AttachFinishedMsgFields::LocalMountPoint, Optional, std::string)
    }));

    static constexpr auto DetachFinishedSchema = SortedSchema("DetachFinished", std::array<FieldDef, 0>{});

    static constexpr auto AuthInfoSchema = SortedSchema("AuthInfo", std::to_array<FieldDef>({
      DEF_FIELD(AuthInfoMsgFields::Username,      Required, std::string),
      DEF_FIELD(AuthInfoMsgFields::Password,      Required, std::string),
      DEF_FIELD(AuthInfoMsgFields::AuthTimestamp, Required, int64_t),
      DEF_FIELD(AuthInfoMsgFields::AuthType,      Optional, std::string)
    }));

    static constexpr auto MediaChangedSchema = SortedSchema("MediaChanged", std::array<FieldDef, 0>{});

    static constexpr auto RedirectSchema = SortedSchema("Redirect", std::to_array<FieldDef>({
      DEF_FIELD(RedirectMsgFields::NewUrl, Required, std::string)
    }));

    static constexpr auto ErrorSchema = SortedSchema("Error", std::to_array<FieldDef>({
      DEF_FIELD(ErrMsgFields::Reason,    Required, std::string),
      DEF_FIELD(ErrMsgFields::History,   Optional, std::string),
      DEF_FIELD(ErrMsgFields::Transient, Optional, bool)
    }));

    static constexpr auto ProvideSchema = SortedSchema("Provide", std::to_array<FieldDef>({
      DEF_FIELD(ProvideMsgFields::Url,               Required, std::string),
      DEF_FIELD(ProvideMsgFields::Filename,          Optional, std::string),
      DEF_FIELD(ProvideMsgFields::DeltaFile,         Optional, std::string),
      DEF_FIELD(ProvideMsgFields::ExpectedFilesize,  Optional, int64_t),
      DEF_FIELD(ProvideMsgFields::CheckExistOnly,    Optional, bool),
      DEF_FIELD(ProvideMsgFields::FileHeaderSize,    Optional, int64_t)
    }));

    static constexpr auto CancelSchema = SortedSchema("Cancel", std::array<FieldDef, 0>{});

    static constexpr auto AttachSchema = SortedSchema("Attach", std::to_array<FieldDef>({
      DEF_FIELD(AttachMsgFields::Url,        Required, std::string),
      DEF_FIELD(AttachMsgFields::AttachId,   Required, std::string),
      DEF_FIELD(AttachMsgFields::Label,      Required, std::string),
      DEF_FIELD(AttachMsgFields::VerifyType, Optional, std::string),
      DEF_FIELD(AttachMsgFields::VerifyData, Optional, std::string),
      DEF_FIELD(AttachMsgFields::MediaNr,    Optional, int32_t),
      DEF_FIELD(AttachMsgFields::Device,     Optional, std::string)
    }), &validateAttachVerification);

    static constexpr auto DetachSchema = SortedSchema("Detach", std::to_array<FieldDef>({
      DEF_FIELD(DetachMsgFields::Url, Required, std::string)
    }));

    static constexpr auto AuthDataRequestSchema = SortedSchema("AuthDataRequest", std::to_array<FieldDef>({
      DEF_FIELD(AuthDataRequestMsgFields::EffectiveUrl,      Required, std::string),
      DEF_FIELD(AuthDataRequestMsgFields::LastAuthTimestamp, Optional, int64_t),
      DEF_FIELD(AuthDataRequestMsgFields::LastUser,          Optional, std::string),
      DEF_FIELD(AuthDataRequestMsgFields::AuthHint,          Optional, std::string)
    }));

    static constexpr auto MediaChangeRequestSchema = SortedSchema("MediaChangeRequest", std::to_array<FieldDef>({
      DEF_FIELD(MediaChangeRequestMsgFields::Label,   Required, std::string),
      DEF_FIELD(MediaChangeRequestMsgFields::MediaNr, Required, int32_t),
      DEF_FIELD(MediaChangeRequestMsgFields::Device,  Required, std::string),
      DEF_FIELD(MediaChangeRequestMsgFields::Desc,    Optional, std::string)
    }));

    // --- Protocol Limits Calculation ---

    static constexpr MessageSchema AllSchemas[] = {
      ProvideStartedSchema, ProvideFinishedSchema, AttachFinishedSchema, DetachFinishedSchema,
      AuthInfoSchema, MediaChangedSchema, RedirectSchema, ErrorSchema, ProvideSchema,
      CancelSchema, AttachSchema, DetachSchema, AuthDataRequestSchema, MediaChangeRequestSchema
    };

    static consteval size_t maxFields() {
      size_t m = 0;
      for (const auto& s : AllSchemas) if (s.fields.size() > m) m = s.fields.size();
      return m;
    }
    static constexpr size_t PROTOCOL_MAX_FIELDS = maxFields();

    /**
     * Optimized Single Engine: Populates a message using binary search over hashes.
     */
    static expected<void> populate( const zypp::PluginFrame &msg, ProvideMessage &pMessage, const MessageSchema &schema )
    {
      std::bitset<PROTOCOL_MAX_FIELDS> foundFields;

      for ( auto it = msg.headerBegin(); it != msg.headerEnd(); ++it ) {
        const auto &name = it->first;
        const auto &val  = it->second;

        if ( name == ProvideMessageFields::RequestCode || name == ProvideMessageFields::RequestId )
          continue;

        uint64_t h = fnv1a(name);
        auto fieldIt = std::lower_bound(schema.fields.begin(), schema.fields.end(), h,
            [](const FieldDef& f, uint64_t hash) { return f.hash < hash; });

        if ( fieldIt != schema.fields.end() && fieldIt->hash == h && fieldIt->name == name ) {
          if ( auto res = fieldIt->parser( val, pMessage, schema.typeName, name ); !res ) return res;
          foundFields.set( std::distance(schema.fields.begin(), fieldIt) );
        } else {
          pMessage.addValue( name, val );
        }
      }

      for ( size_t i = 0; i < schema.fields.size(); ++i ) {
        if ( schema.fields[i].rule == Required && !foundFields.test(i) ) {
          return expected<void>::error( ZYPP_EXCPT_PTR( InvalidMessageReceivedException(
            zypp::str::Str() << schema.typeName << " message is missing required field: " << schema.fields[i].name ) ) );
        }
      }

      return schema.customValidate ? schema.customValidate( pMessage ) : expected<void>::success();
    }
  }

  // --- ProvideMessage Public Factory and Accessors ---

  ProvideMessage::ProvideMessage() { }

  expected<ProvideMessage> ProvideMessage::create( const zypp::PluginFrame &msg )
  {
    if ( msg.command() != ProvideMessage::typeName ) {
      return expected<ProvideMessage>::error( ZYPP_EXCPT_PTR( InvalidMessageReceivedException("Message type mismatch") ) );
    }

    const std::string &codeStr = msg.getHeaderNT( std::string(ProvideMessageFields::RequestCode) );
    const std::string &idStr   = msg.getHeaderNT( std::string(ProvideMessageFields::RequestId) );

    if ( codeStr.empty() || idStr.empty() ) {
      return expected<ProvideMessage>::error( ZYPP_EXCPT_PTR( InvalidMessageReceivedException("Missing envelope headers")) );
    }

    const auto code = static_cast<MessageCodes>( zyppng::str::strict_strtonum<uint32_t>( codeStr ).value_or( 0 ) );
    const auto id   = zyppng::str::strict_strtonum<uint32_t>( idStr ).value_or( 0 );

    ProvideMessage pMessage;
    pMessage.setCode( code );
    pMessage.setRequestId( id );

    expected<void> res;
    switch ( code ) {
      case Code::ProvideStarted:    res = populate( msg, pMessage, ProvideStartedSchema ); break;
      case Code::ProvideFinished:   res = populate( msg, pMessage, ProvideFinishedSchema ); break;
      case Code::AttachFinished:    res = populate( msg, pMessage, AttachFinishedSchema ); break;
      case Code::DetachFinished:    res = populate( msg, pMessage, DetachFinishedSchema ); break;
      case Code::AuthInfo:          res = populate( msg, pMessage, AuthInfoSchema ); break;
      case Code::MediaChanged:      res = populate( msg, pMessage, MediaChangedSchema ); break;
      case Code::Redirect:          res = populate( msg, pMessage, RedirectSchema ); break;
      case Code::Prov:              res = populate( msg, pMessage, ProvideSchema ); break;
      case Code::Cancel:            res = populate( msg, pMessage, CancelSchema ); break;
      case Code::Attach:            res = populate( msg, pMessage, AttachSchema ); break;
      case Code::Detach:            res = populate( msg, pMessage, DetachSchema ); break;
      case Code::AuthDataRequest:   res = populate( msg, pMessage, AuthDataRequestSchema ); break;
      case Code::MediaChangeRequest:res = populate( msg, pMessage, MediaChangeRequestSchema ); break;

      default:
        if ( code >= Code::FirstClientErrCode && code <= Code::LastSrvErrCode ) {
          res = populate( msg, pMessage, ErrorSchema );
        } else {
          return expected<ProvideMessage>::error( ZYPP_EXCPT_PTR( InvalidMessageReceivedException("Unknown protocol code") ) );
        }
    }

    if ( !res ) return expected<ProvideMessage>::error( res.error() );
    return pMessage;
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
    for ( const auto &hdr : extraValues ) {
      msg.setValue ( hdr.first, hdr.second );
    }

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

} // namespace zyppng
