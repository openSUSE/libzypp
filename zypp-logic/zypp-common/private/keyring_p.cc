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


  void KeyRingImpl::importKey( const PublicKey & key, const Ring ring )
  {
    MIL << "Imported key " << key << " to " << ring << endl;

    if ( ring == Ring::Trusted )
    {
      if ( key.hiddenKeys().empty() )
      {
        _sigTrustedKeyAdded.emit( key );
      }
      else
      {
        // multiple keys: Export individual keys ascii armored to import in rpmdb
        _sigTrustedKeyAdded.emit( exportKey( key, Ring::Trusted ) );
        for ( const PublicKeyData & hkey : key.hiddenKeys() )
          _sigTrustedKeyAdded.emit( exportKey( hkey, Ring::Trusted ) );
      }
    }
  }

  // TODO: (ma) check for a workflow where one context imports multiple keys.
  void KeyRingImpl::importKeys( const std::list<PublicKey> & keys, const Ring ring )
  {
     for ( const PublicKey & key : keys )
       importKey( key, ring );
  }

  void KeyRingImpl::multiKeyImport( const Pathname & keyfile_r, const Ring ring )
  {
    importKey( keyfile_r, keyRingPath( ring ) );
  }


  void KeyRingImpl::deleteKey( const std::string & id, const Ring ring )
  {
    PublicKeyData keyDataToDel( publicKeyData( id, ring ) );
    if ( ! keyDataToDel )
    {
      WAR << "Key to delete [" << id << "] is not in " << ring << endl;
      return;
    }

    deleteKey( id, keyRingPath( ring ) );
    MIL << "Deleted key [" << id << "] from " << ring << endl;

    if ( ring == Ring::Trusted ) {
      _sigTrustedKeyAdded.emit ( PublicKey( keyDataToDel ) );
    }
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

  PublicKey KeyRingImpl::exportKey( const PublicKeyData & keyData, const Pathname & keyring ) const
  {
    return PublicKey( dumpPublicKeyToTmp( keyData.id(), keyring ), keyData );
  }

  // TODO: (ma) not obvious why "exportKey id" checks validity, but "exportKey keyData" does not
  PublicKey KeyRingImpl::exportKey( const std::string & id, const Pathname & keyring ) const
  {
    PublicKeyData keyData( publicKeyData( id, keyring ) );
    if ( keyData )
      return PublicKey( dumpPublicKeyToTmp( keyData.id(), keyring ), keyData );

    // Here: key not found
    WAR << "No key [" << id << "] to export from " << keyring << endl;
    return PublicKey();
  }

  void KeyRingImpl::deleteKey( const std::string & id, const Pathname & keyring )
  {
    CachedPublicKeyData::Manip manip { keyRingManip( keyring ) }; // Provides the context if we want to manip a cached keyring.
    if ( ! manip.keyManagerCtx().deleteKey( id ) )
      ZYPP_THROW(KeyRingException(_("Failed to delete key.")));
  }


  PublicKeyData KeyRingImpl::publicKeyData( const std::string & id, const Pathname & keyring ) const
  {
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

  std::list<PublicKey> KeyRingImpl::publicKeys( const Pathname & keyring ) const
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


  void KeyRingImpl::dumpPublicKey( const std::string & id, const Pathname & keyring, std::ostream & stream ) const
  {
    KeyManagerCtx::createForOpenPGP( keyring ).exportKey(id, stream);
  }

  filesystem::TmpFile KeyRingImpl::dumpPublicKeyToTmp( const std::string & id, const Pathname & keyring ) const
  {
    filesystem::TmpFile tmpFile( _base_dir, "pubkey-"+id+"-" );
    MIL << "Going to export key [" << id << "] from " << keyring << " to " << tmpFile.path() << endl;

    std::ofstream os( tmpFile.path().c_str() );
    dumpPublicKey( id, keyring, os );
    os.close();
    return tmpFile;
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
  { return KeyManagerCtx::createForOpenPGP( keyring ).verify( file, signature ); }


  void KeyRingImpl::preloadCachedKeys()
  {
    // For now just load the 'gpg-pubkey-*.{asc,key}' files into the general keyring.
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

    // We load all the matching files. Although the key-id embedded in the filename
    // suggests there's just one key inside, this must not be true. There may be
    // more keys hidden in the file (see PublicKey::hiddenKeys). And more important,
    // there my be trusted keys with with an extended lifetime. They need to be updated
    // on the fly in the trusteddb.
    std::list<PublicKey> newkeys;
    for ( const auto & cache : cachedirs ) {
      dirForEach( cache,
                  [&newkeys]( const Pathname & dir_r, const char *const file_r )->bool {
                    static const str::regex rx { "^gpg-pubkey-([[:xdigit:]]{8,})(-[[:xdigit:]]{8,})?\\.(asc|key)$" };
                    str::smatch what;
                    if ( str::regex_match( file_r, what, rx ) ) {
                      newkeys.push_back( PublicKey( dir_r / file_r ) );
                    }
                    return true;
                  }
                );
    }
    if ( not newkeys.empty() ) {
      MIL << "preload cached keys..." << endl;
      importKeys( newkeys, Ring::General );
    }
  }

} // namespace zypp

