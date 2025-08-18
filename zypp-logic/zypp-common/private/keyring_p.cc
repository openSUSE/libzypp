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

#include "keyring_p.h"

#include <iostream>
#include <fstream>
#include <optional>
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
#include <zypp-common/KeyManager.h>

using std::endl;

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::KeyRing"

///////////////////////////////////////////////////////////////////
namespace zypp
{
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


  KeyRingImpl::KeyRingImpl(const filesystem::Pathname &baseTmpDir)
    : _trusted_tmp_dir( baseTmpDir, "zypp-trusted-kr" )
    , _general_tmp_dir( baseTmpDir, "zypp-general-kr" )
    , _base_dir( baseTmpDir )
  {
  }

  void KeyRingImpl::importKey( const PublicKey & key, bool trusted )
  {
    importKey( key.path(), trusted ? trustedKeyRing() : generalKeyRing() );
    MIL << "Imported key " << key << " to " << (trusted ? "trustedKeyRing" : "generalKeyRing" ) << endl;

    if ( trusted )
    {
      if ( key.hiddenKeys().empty() )
      {
        _sigTrustedKeyAdded.emit( key );
      }
      else
      {
        // multiple keys: Export individual keys ascii armored to import in rpmdb
        _sigTrustedKeyAdded.emit( exportKey( key, trustedKeyRing() ) );
        for ( const PublicKeyData & hkey : key.hiddenKeys() )
          _sigTrustedKeyAdded.emit( exportKey( hkey, trustedKeyRing() ) );
      }
    }
  }

  void KeyRingImpl::multiKeyImport( const Pathname & keyfile_r, bool trusted_r )
  {
    importKey( keyfile_r, trusted_r ? trustedKeyRing() : generalKeyRing() );
  }

  void KeyRingImpl::deleteKey( const std::string & id, bool trusted )
  {
    PublicKeyData keyDataToDel( publicKeyExists( id, trusted ? trustedKeyRing() : generalKeyRing() ) );
    if ( ! keyDataToDel )
    {
      WAR << "Key to delete [" << id << "] is not in " << (trusted ? "trustedKeyRing" : "generalKeyRing" ) << endl;
      return;
    }

    deleteKey( id, trusted ? trustedKeyRing() : generalKeyRing() );
    MIL << "Deleted key [" << id << "] from " << (trusted ? "trustedKeyRing" : "generalKeyRing" ) << endl;

    if ( trusted ) {
      _sigTrustedKeyAdded.emit ( PublicKey( keyDataToDel ) );
    }
  }

  PublicKeyData KeyRingImpl::publicKeyExists( const std::string & id, const Pathname & keyring )
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

  void KeyRingImpl::preloadCachedKeys()
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

  PublicKey KeyRingImpl::exportKey( const PublicKeyData & keyData, const Pathname & keyring )
  {
    return PublicKey( dumpPublicKeyToTmp( keyData.id(), keyring ), keyData );
  }

  PublicKey KeyRingImpl::exportKey( const std::string & id, const Pathname & keyring )
  {
    PublicKeyData keyData( publicKeyExists( id, keyring ) );
    if ( keyData )
      return PublicKey( dumpPublicKeyToTmp( keyData.id(), keyring ), keyData );

    // Here: key not found
    WAR << "No key [" << id << "] to export from " << keyring << endl;
    return PublicKey();
  }


  void KeyRingImpl::dumpPublicKey( const std::string & id, const Pathname & keyring, std::ostream & stream )
  {
    KeyManagerCtx::createForOpenPGP( keyring ).exportKey(id, stream);
  }

  filesystem::TmpFile KeyRingImpl::dumpPublicKeyToTmp( const std::string & id, const Pathname & keyring )
  {
    filesystem::TmpFile tmpFile( _base_dir, "pubkey-"+id+"-" );
    MIL << "Going to export key [" << id << "] from " << keyring << " to " << tmpFile.path() << endl;

    std::ofstream os( tmpFile.path().c_str() );
    dumpPublicKey( id, keyring, os );
    os.close();
    return tmpFile;
  }

  std::list<PublicKey> KeyRingImpl::publicKeys( const Pathname & keyring )
  {
    const std::list<PublicKeyData> & keys( publicKeyData( keyring ) );
    std::list<PublicKey> ret;

    for ( const PublicKeyData& keyData : keys )
    {
      PublicKey key( exportKey( keyData, keyring ) );
      ret.push_back( key );
      MIL << "Found key " << key << endl;
    }
    return ret;
  }

  void KeyRingImpl::importKey( const Pathname & keyfile, const Pathname & keyring )
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

  void KeyRingImpl::deleteKey( const std::string & id, const Pathname & keyring )
  {
    CachedPublicKeyData::Manip manip { keyRingManip( keyring ) }; // Provides the context if we want to manip a cached keyring.
    if ( ! manip.keyManagerCtx().deleteKey( id ) )
      ZYPP_THROW(KeyRingException(_("Failed to delete key.")));
  }

  std::string KeyRingImpl::readSignatureKeyId( const Pathname & signature )
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

  bool KeyRingImpl::verifyFile( const Pathname & file, const Pathname & signature, const Pathname & keyring )
  {
    return KeyManagerCtx::createForOpenPGP( keyring ).verify( file, signature );
  }

} // namespace zypp

