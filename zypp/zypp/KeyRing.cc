/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/KeyRing.cc
 *
*/

#include "zypp_detail/keyring_p.h"

#include <iostream>
#include <sys/file.h>
#include <unistd.h>

#include <zypp/TmpPath.h>
#include <zypp/ZYppFactory.h>
#include <zypp/ZYpp.h>

#include <zypp-core/base/LogTools.h>
#include <zypp-core/base/IOStream.h>
#include <zypp-core/base/String.h>
#include <zypp-core/base/Regex.h>
#include <zypp-core/base/Gettext.h>
#include <zypp-core/fs/WatchFile>
#include <zypp/PathInfo.h>
#include <zypp-core/ExternalProgram.h>
#include <zypp/TmpPath.h>
#include <zypp/ZYppCallbacks.h>       // JobReport::instance
#include <zypp-common/KeyManager.h>

#include <zypp/ng/workflows/keyringwf.h>

using std::endl;

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::KeyRing"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  IMPL_PTR_TYPE(KeyRing);

  namespace
  {
    KeyRing::DefaultAccept _keyRingDefaultAccept( KeyRing::ACCEPT_NOTHING );
  }

  KeyRing::DefaultAccept KeyRing::defaultAccept()
  { return _keyRingDefaultAccept; }

  void KeyRing::setDefaultAccept( DefaultAccept value_r )
  {
    MIL << "Set new KeyRing::DefaultAccept: " << value_r << endl;
    _keyRingDefaultAccept = value_r;
  }

  void KeyRingReport::infoVerify( const std::string & file_r, const PublicKeyData & keyData_r, const KeyContext & keycontext )
  {}

  bool KeyRingReport::askUserToAcceptUnsignedFile( const std::string & file, const KeyContext & keycontext )
  { return _keyRingDefaultAccept.testFlag( KeyRing::ACCEPT_UNSIGNED_FILE ); }

  KeyRingReport::KeyTrust
  KeyRingReport::askUserToAcceptKey( const PublicKey & key, const KeyContext & keycontext )
  {
    if ( _keyRingDefaultAccept.testFlag( KeyRing::TRUST_KEY_TEMPORARILY ) )
      return KEY_TRUST_TEMPORARILY;
    if ( _keyRingDefaultAccept.testFlag( KeyRing::TRUST_AND_IMPORT_KEY ) )
      return KEY_TRUST_AND_IMPORT;
    return KEY_DONT_TRUST;
  }

  bool KeyRingReport::askUserToAcceptUnknownKey( const std::string & file, const std::string & id, const KeyContext & keycontext )
  { return _keyRingDefaultAccept.testFlag( KeyRing::ACCEPT_UNKNOWNKEY ); }

  bool KeyRingReport::askUserToAcceptVerificationFailed( const std::string & file, const PublicKey & key, const KeyContext & keycontext )
  { return _keyRingDefaultAccept.testFlag( KeyRing::ACCEPT_VERIFICATION_FAILED ); }

  bool KeyRingReport::askUserToAcceptPackageKey(const PublicKey &key_r, const KeyContext &keycontext_r)
  {
    UserData data(ACCEPT_PACKAGE_KEY_REQUEST);
    data.set("PublicKey", key_r);
    data.set("KeyContext", keycontext_r);
    report(data);

    if ( data.hasvalue("TrustKey") )
      return data.get<bool>("TrustKey");
    return false;
  }

  void KeyRingReport::reportNonImportedKeys(const std::set<Edition> &keys_r)
  {
    UserData data(KEYS_NOT_IMPORTED_REPORT);
    data.set("Keys", keys_r);
    report(data);
  }

  void KeyRingReport::reportAutoImportKey( const std::list<PublicKeyData> & keyDataList_r,
                                           const PublicKeyData & keySigning_r,
                                           const KeyContext &keyContext_r )
  {
    UserData data { REPORT_AUTO_IMPORT_KEY };
    data.set( "KeyDataList", keyDataList_r );
    data.set( "KeySigning",  keySigning_r );
    data.set( "KeyContext",  keyContext_r );
    report( data );
  }

  namespace
  {
    /// Handle signal emission from within KeyRing::Impl::importKey
    struct ImportKeyCBHelper
    {
      void operator()( const PublicKey & key_r )
      {
        try {
          _rpmdbEmitSignal->trustedKeyAdded( key_r );
          _emitSignal->trustedKeyAdded( key_r );
        }
        catch ( const Exception & excp )
        {
          ERR << "Could not import key into rpmdb: " << excp << endl;
          // TODO: JobReport as hotfix for bsc#1057188; should bubble up and go through some callback
          JobReport::error( excp.asUserHistory() );
        }
      }

    private:
      callback::SendReport<target::rpm::KeyRingSignals> _rpmdbEmitSignal;
      callback::SendReport<KeyRingSignals>              _emitSignal;
    };
  } // namespace

  KeyRing::Impl::Impl(const filesystem::Pathname &baseTmpDir)
    : KeyRingImpl( baseTmpDir )
  {
    MIL << "Current KeyRing::DefaultAccept: " << _keyRingDefaultAccept << std::endl;

    sigTrustedKeyAdded ().connect ([]( const PublicKey &key ){
      ImportKeyCBHelper emit;
      emit(key);
    });

    sigTrustedKeyRemoved ().connect ([]( const PublicKey &key ){
      try {
        callback::SendReport<target::rpm::KeyRingSignals> rpmdbEmitSignal;
        rpmdbEmitSignal->trustedKeyRemoved( key );

        callback::SendReport<KeyRingSignals> emitSignal;
        emitSignal->trustedKeyRemoved( key );
      }
      catch ( const Exception & excp )
      {
        ERR << "Could not delete key from rpmmdb: " << excp << endl;
        // TODO: JobReport as hotfix for bsc#1057188; should bubble up and go through some callback
        JobReport::error( excp.asUserHistory() );
      }
    });
  }


  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : KeyRing
  //
  ///////////////////////////////////////////////////////////////////

  KeyRing::KeyRing( const Pathname & baseTmpDir )
  : _pimpl( new Impl( baseTmpDir ) )
  {}

  KeyRing::~KeyRing()
  {}

  KeyRing::Impl &KeyRing::pimpl()
  {
    return *_pimpl;
  }

  void KeyRing::allowPreload( bool yesno_r )
  { _pimpl->allowPreload( yesno_r ); }


  void KeyRing::importKey( const PublicKey & key, bool trusted )
  { _pimpl->importKey( key, trusted ); }

  void KeyRing::multiKeyImport( const Pathname & keyfile_r, bool trusted_r )
  { _pimpl->multiKeyImport( keyfile_r, trusted_r ); }

  std::string KeyRing::readSignatureKeyId( const Pathname & signature )
  { return _pimpl->readSignatureKeyId( signature ); }

  void KeyRing::deleteKey( const std::string & id, bool trusted )
  { _pimpl->deleteKey( id, trusted ); }

  std::list<PublicKey> KeyRing::publicKeys()
  { return _pimpl->publicKeys(); }

  std:: list<PublicKey> KeyRing::trustedPublicKeys()
  { return _pimpl->trustedPublicKeys(); }

  std::list<PublicKeyData> KeyRing::publicKeyData()
  { return _pimpl->publicKeyData(); }

  std::list<PublicKeyData> KeyRing::trustedPublicKeyData()
  { return _pimpl->trustedPublicKeyData(); }

  PublicKeyData KeyRing::publicKeyData( const std::string &id_r )
  { return _pimpl->publicKeyExists( id_r ); }

  PublicKeyData KeyRing::trustedPublicKeyData(const std::string &id_r)
  { return _pimpl->trustedPublicKeyExists( id_r ); }

  bool KeyRing::verifyFileSignature( const Pathname & file, const Pathname & signature )
  { return _pimpl->verifyFileSignature( file, signature ); }

  bool KeyRing::verifyFileTrustedSignature( const Pathname & file, const Pathname & signature )
  { return _pimpl->verifyFileTrustedSignature( file, signature ); }
  void KeyRing::dumpPublicKey( const std::string & id, bool trusted, std::ostream & stream )
  { _pimpl->dumpPublicKey( id, trusted, stream ); }

  PublicKey KeyRing::exportPublicKey( const PublicKeyData & keyData )
  { return _pimpl->exportPublicKey( keyData ); }

  PublicKey KeyRing::exportTrustedPublicKey( const PublicKeyData & keyData )
  { return _pimpl->exportTrustedPublicKey( keyData ); }

  bool KeyRing::isKeyTrusted( const std::string & id )
  { return _pimpl->isKeyTrusted( id ); }

  bool KeyRing::isKeyKnown( const std::string & id )
  { return _pimpl->isKeyKnown( id ); }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
