/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "userrequest.h"

namespace zyppng {

  UserData AcceptNoDigestRequest::makeData(const zypp::Pathname &p) {
    UserData d( CTYPE.data() );
    d.set( FILE.data(), p );
    return d;
  }

  UserData AcceptUnknownDigestRequest::makeData(const zypp::Pathname &p, const std::string &name) {
    UserData d( CTYPE.data() );
    d.set( FILE.data(), p );
    d.set( NAME.data(), name );
    return d;
  }

  UserData AcceptWrongDigestRequest::makeData(const zypp::Pathname &p, const std::string &requested, const std::string &found) {
    UserData d( CTYPE.data() );
    d.set( FILE.data(), p );
    d.set( NAME_REQUESTED.data(), requested );
    d.set( NAME_FOUND.data(), found );
    return d;
  }

  UserData AcceptKeyRequest::makeData(const zypp::PublicKey &key, const zypp::KeyContext &keycontext)
  {
    UserData d( CTYPE.data() );
    d.set( KEY.data(), key );
    d.set( KEY_CONTEXT.data(), keycontext );
    return d;
  }

  UserData VerifyInfoEvent::makeData(const std::string &file_r, const zypp::PublicKeyData &keyData_r, const zypp::KeyContext &keycontext)
  {
    UserData d( CTYPE.data() );
    d.set( FILE.data(), file_r );
    d.set( KEY_DATA.data(), keyData_r );
    d.set( KEY_CONTEXT.data(), keycontext );
    return d;
  }

  UserData AcceptUnsignedFileRequest::makeData(const std::string &file, const zypp::KeyContext &keycontext)
  {
    UserData d( CTYPE.data() );
    d.set( FILE.data(), file );
    d.set( KEY_CONTEXT.data(), keycontext );
    return d;
  }

  UserData AcceptUnknownKeyRequest::makeData(const std::string &file, const std::string &id, const zypp::KeyContext &keycontext)
  {
    UserData d( CTYPE.data() );
    d.set( KEYID.data(), id );
    d.set( FILE.data(), file );
    d.set( KEY_CONTEXT.data(), keycontext );
    return d;
  }

  UserData AcceptFailedVerificationRequest::makeData(const std::string &file, const zypp::PublicKey &key, const zypp::KeyContext &keycontext)
  {
    UserData d( CTYPE.data() );
    d.set( KEY.data(), key );
    d.set( FILE.data(), file );
    d.set( KEY_CONTEXT.data(), keycontext );
    return d;
  }

  UserData AcceptPackageKeyRequest::makeData(const zypp::PublicKey &key_r, const zypp::KeyContext &keycontext_r)
  {
    UserData d( CTYPE.data() );
    d.set( KEY.data(), key_r );
    d.set( KEY_CONTEXT.data(), keycontext_r );
    return d;
  }

  UserData NonImportedKeysInfoEvent::makeData(const std::set<zypp::Edition> &keys_r)
  {
    UserData d( CTYPE.data() );
    d.set( KEYS.data(), &keys_r );
    return d;
  }

  UserData KeyAutoImportInfoEvent::makeData(const std::list<zypp::PublicKeyData> &keyDataList_r, const zypp::PublicKeyData &keySigning_r, const zypp::KeyContext &keyContext_r)
  {
    UserData d( CTYPE.data() );
    d.set( KEY_DATA_LIST.data(), &keyDataList_r );
    d.set( KEY_DATA.data(), keySigning_r );
    d.set( KEY_CONTEXT.data(), keyContext_r );
    return d;
  }
}
