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
#include <fstream>
#include <optional>
#include <sys/file.h>
#include <cstdio>
#include <unistd.h>

#include <zypp/TmpPath.h>
#include <zypp/ZYppFactory.h>
#include <zypp/ZYpp.h>

#include <zypp/base/LogTools.h>
#include <zypp/base/IOStream.h>
#include <zypp/base/String.h>
#include <zypp/base/Regex.h>
#include <zypp/base/Gettext.h>
#include <zypp-core/fs/WatchFile>
#include <zypp/PathInfo.h>
#include <zypp/ExternalProgram.h>
#include <zypp/TmpPath.h>
#include <zypp/ZYppCallbacks.h>       // JobReport::instance
#include <zypp/KeyManager.h>

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

  ///////////////////////////////////////////////////////////////////

  CachedPublicKeyData::Manip::Manip(CachedPublicKeyData &cache_r, filesystem::Pathname keyring_r)
    : _cache { cache_r }
    , _keyring { std::move(keyring_r) }
  {}

  KeyManagerCtx &CachedPublicKeyData::Manip::keyManagerCtx() {
    if ( not _context ) {
      _context = KeyManagerCtx::createForOpenPGP( _keyring );
    }
    // frankly: don't remember why an explicit setDirty was introduced and
    // why WatchFile was not enough. Maybe some corner case when the keyrings
    // are created?
    _cache.setDirty( _keyring );
    return _context.value();
  }

  CachedPublicKeyData::Cache::Cache() {}

  void CachedPublicKeyData::Cache::setDirty()
  {
    _keyringK.reset();
    _keyringP.reset();
  }

  void CachedPublicKeyData::Cache::assertCache(const filesystem::Pathname &keyring_r)
  {
    // .kbx since gpg2-2.1
    if ( !_keyringK )
      _keyringK.reset( new WatchFile( keyring_r/"pubring.kbx", WatchFile::NO_INIT ) );
    if ( !_keyringP )
      _keyringP.reset( new WatchFile( keyring_r/"pubring.gpg", WatchFile::NO_INIT ) );
  }

  bool CachedPublicKeyData::Cache::hasChanged() const
  {
    bool k = _keyringK->hasChanged();	// be sure both files are checked
    bool p = _keyringP->hasChanged();
    return k || p;
  }

  const std::list<PublicKeyData> &CachedPublicKeyData::operator()(const filesystem::Pathname &keyring_r) const
  { return getData( keyring_r ); }

  void CachedPublicKeyData::setDirty(const filesystem::Pathname &keyring_r)
  { _cacheMap[keyring_r].setDirty(); }

  CachedPublicKeyData::Manip CachedPublicKeyData::manip(filesystem::Pathname keyring_r) { return Manip( *this, std::move(keyring_r) ); }

  const std::list<PublicKeyData> &CachedPublicKeyData::getData(const filesystem::Pathname &keyring_r) const
  {
    Cache & cache( _cacheMap[keyring_r] );
    // init new cache entry
    cache.assertCache( keyring_r );
    return getData( keyring_r, cache );
  }

  const std::list<PublicKeyData> &CachedPublicKeyData::getData(const filesystem::Pathname &keyring_r, Cache &cache_r) const
  {
    if ( cache_r.hasChanged() ) {
      cache_r._data = KeyManagerCtx::createForOpenPGP( keyring_r ).listKeys();
      MIL << "Found keys: " << cache_r._data  << std::endl;
    }
    return cache_r._data;
  }


  ///////////////////////////////////////////////////////////////////

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
    : _trusted_tmp_dir( baseTmpDir, "zypp-trusted-kr" )
    , _general_tmp_dir( baseTmpDir, "zypp-general-kr" )
    , _base_dir( baseTmpDir )
  {
    MIL << "Current KeyRing::DefaultAccept: " << _keyRingDefaultAccept << std::endl;
  }

  void KeyRing::Impl::importKey( const PublicKey & key, bool trusted )
  {
    importKey( key.path(), trusted ? trustedKeyRing() : generalKeyRing() );
    MIL << "Imported key " << key << " to " << (trusted ? "trustedKeyRing" : "generalKeyRing" ) << endl;

    if ( trusted )
    {
      ImportKeyCBHelper emitSignal;
      if ( key.hiddenKeys().empty() )
      {
        emitSignal( key );
      }
      else
      {
        // multiple keys: Export individual keys ascii armored to import in rpmdb
        emitSignal( exportKey( key, trustedKeyRing() ) );
        for ( const PublicKeyData & hkey : key.hiddenKeys() )
          emitSignal( exportKey( hkey, trustedKeyRing() ) );
      }
    }
  }

  void KeyRing::Impl::multiKeyImport( const Pathname & keyfile_r, bool trusted_r )
  {
    importKey( keyfile_r, trusted_r ? trustedKeyRing() : generalKeyRing() );
  }

  void KeyRing::Impl::deleteKey( const std::string & id, bool trusted )
  {
    PublicKeyData keyDataToDel( publicKeyExists( id, trusted ? trustedKeyRing() : generalKeyRing() ) );
    if ( ! keyDataToDel )
    {
      WAR << "Key to delete [" << id << "] is not in " << (trusted ? "trustedKeyRing" : "generalKeyRing" ) << endl;
      return;
    }
    deleteKey( id, trusted ? trustedKeyRing() : generalKeyRing() );
    MIL << "Deleted key [" << id << "] from " << (trusted ? "trustedKeyRing" : "generalKeyRing" ) << endl;

    if ( trusted )
    try {
      PublicKey key( keyDataToDel );

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
  }

  PublicKeyData KeyRing::Impl::publicKeyExists( const std::string & id, const Pathname & keyring )
  {
    if ( _allowPreload && keyring == generalKeyRing() ) {
      _allowPreload = false;
      preloadCachedKeys();
    }

    PublicKeyData ret;
    for ( const PublicKeyData & key : publicKeyData( keyring ) )
    {
      if ( key.providesKey( id ) )
      {
        ret = key;
        break;
      }
    }
    DBG << (ret ? "Found" : "No") << " key [" << id << "] in keyring " << keyring << endl;
    return ret;
  }

  void KeyRing::Impl::preloadCachedKeys()
  {
    MIL << "preloadCachedKeys into general keyring..." << endl;
    CachedPublicKeyData::Manip manip { keyRingManip( generalKeyRing() ) }; // Provides the context if we want to manip a cached keyring.

    // For now just load the 'gpg-pubkey-*.{asc,key}' files into the general keyring,
    // if their id (derived from the filename) is not in the trusted ring.
    // TODO: Head for a persistent general keyring.
    std::set<Pathname> cachedirs;
    ZConfig & conf { ZConfig::instance() };
    cachedirs.insert( conf.pubkeyCachePath() );
    cachedirs.insert( "/usr/lib/rpm/gnupg/keys" );
    if ( Pathname r = conf.systemRoot(); r != "/" && not r.empty() ) {
      cachedirs.insert( r / conf.pubkeyCachePath() );
      cachedirs.insert( r / "/usr/lib/rpm/gnupg/keys" );
    }
    if ( Pathname r = conf.repoManagerRoot(); r != "/" && not r.empty() ) {
      cachedirs.insert( r / conf.pubkeyCachePath() );
      cachedirs.insert( r / "/usr/lib/rpm/gnupg/keys" );
    }

    std::map<std::string,Pathname> keyCandidates;	// Collect one file path per keyid
    const str::regex rx { "^gpg-pubkey-([[:xdigit:]]{8,})(-[[:xdigit:]]{8,})?\\.(asc|key)$" };
    for ( const auto & cache : cachedirs ) {
      dirForEach( cache,
                  [&rx,&keyCandidates]( const Pathname & dir_r, const char *const file_r )->bool {
                    str::smatch what;
                    if ( str::regex_match( file_r, what, rx ) ) {
                      Pathname & remember { keyCandidates[what[1]] };
                      if ( remember.empty() ) {
                        remember = dir_r / file_r;
                      }
                    }
                    return true;
                  }
                );
    }

    for ( const auto & p : keyCandidates ) {
      // Avoid checking the general keyring while it is flagged dirty.
      // Checking the trusted ring is ok, and most keys will be there anyway.
      const std::string & id { p.first };
      const Pathname & path { p.second };
      if ( isKeyTrusted(id) )
        continue;
      if ( manip.keyManagerCtx().importKey( path ) ) {
        DBG << "preload key file " << path << endl;
      }
      else {
        WAR << "Skipping: Can't preload key file " << path << endl;
      }
    }
  }

  PublicKey KeyRing::Impl::exportKey( const PublicKeyData & keyData, const Pathname & keyring )
  {
    return PublicKey( dumpPublicKeyToTmp( keyData.id(), keyring ), keyData );
  }

  PublicKey KeyRing::Impl::exportKey( const std::string & id, const Pathname & keyring )
  {
    PublicKeyData keyData( publicKeyExists( id, keyring ) );
    if ( keyData )
      return PublicKey( dumpPublicKeyToTmp( keyData.id(), keyring ), keyData );

    // Here: key not found
    WAR << "No key [" << id << "] to export from " << keyring << endl;
    return PublicKey();
  }


  void KeyRing::Impl::dumpPublicKey( const std::string & id, const Pathname & keyring, std::ostream & stream )
  {
    KeyManagerCtx::createForOpenPGP( keyring ).exportKey(id, stream);
  }

  filesystem::TmpFile KeyRing::Impl::dumpPublicKeyToTmp( const std::string & id, const Pathname & keyring )
  {
    filesystem::TmpFile tmpFile( _base_dir, "pubkey-"+id+"-" );
    MIL << "Going to export key [" << id << "] from " << keyring << " to " << tmpFile.path() << endl;

    std::ofstream os( tmpFile.path().c_str() );
    dumpPublicKey( id, keyring, os );
    os.close();
    return tmpFile;
  }

  std::list<PublicKey> KeyRing::Impl::publicKeys( const Pathname & keyring )
  {
    const std::list<PublicKeyData> & keys( publicKeyData( keyring ) );
    std::list<PublicKey> ret;

    for_( it, keys.begin(), keys.end() )
    {
      PublicKey key( exportKey( *it, keyring ) );
      ret.push_back( key );
      MIL << "Found key " << key << endl;
    }
    return ret;
  }

  void KeyRing::Impl::importKey( const Pathname & keyfile, const Pathname & keyring )
  {
    if ( ! PathInfo( keyfile ).isExist() )
      // TranslatorExplanation first %s is key name, second is keyring name
      ZYPP_THROW(KeyRingException( str::Format(_("Tried to import not existent key %s into keyring %s"))
                                   % keyfile.asString()
                                   % keyring.asString() ));

    CachedPublicKeyData::Manip manip { keyRingManip( keyring ) }; // Provides the context if we want to manip a cached keyring.
    if ( ! manip.keyManagerCtx().importKey( keyfile ) )
      ZYPP_THROW(KeyRingException(_("Failed to import key.")));
  }

  void KeyRing::Impl::deleteKey( const std::string & id, const Pathname & keyring )
  {
    CachedPublicKeyData::Manip manip { keyRingManip( keyring ) }; // Provides the context if we want to manip a cached keyring.
    if ( ! manip.keyManagerCtx().deleteKey( id ) )
      ZYPP_THROW(KeyRingException(_("Failed to delete key.")));
  }

  std::string KeyRing::Impl::readSignatureKeyId( const Pathname & signature )
  {
    if ( ! PathInfo( signature ).isFile() )
      ZYPP_THROW(KeyRingException( str::Format(_("Signature file %s not found")) % signature.asString() ));

    MIL << "Determining key id of signature " << signature << endl;

    std::list<std::string> fprs = KeyManagerCtx::createForOpenPGP().readSignatureFingerprints( signature );
    if ( ! fprs.empty() ) {
      std::string &id = fprs.back();
      MIL << "Determined key id [" << id << "] for signature " << signature << endl;
      return id;
    }
    return std::string();
  }

  bool KeyRing::Impl::verifyFile( const Pathname & file, const Pathname & signature, const Pathname & keyring )
  {
    return KeyManagerCtx::createForOpenPGP( keyring ).verify( file, signature );
  }

  ///////////////////////////////////////////////////////////////////

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
