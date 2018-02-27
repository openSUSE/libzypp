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
#include <iostream>
#include <fstream>
#include <sys/file.h>
#include <cstdio>
#include <unistd.h>

#include "zypp/TmpPath.h"
#include "zypp/ZYppFactory.h"
#include "zypp/ZYpp.h"

#include "zypp/base/LogTools.h"
#include "zypp/base/IOStream.h"
#include "zypp/base/String.h"
#include "zypp/base/Regex.h"
#include "zypp/base/Gettext.h"
#include "zypp/base/WatchFile.h"
#include "zypp/PathInfo.h"
#include "zypp/KeyRing.h"
#include "zypp/ExternalProgram.h"
#include "zypp/TmpPath.h"
#include "zypp/ZYppCallbacks.h"       // JobReport::instance
#include "zypp/KeyManager.h"

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

  namespace
  {
    ///////////////////////////////////////////////////////////////////
    /// \class CachedPublicKeyData
    /// \brief Functor returning the keyrings data (cached).
    /// \code
    ///   const std::list<PublicKeyData> & cachedPublicKeyData( const Pathname & keyring );
    /// \endcode
    ///////////////////////////////////////////////////////////////////
    struct CachedPublicKeyData : private base::NonCopyable
    {
      const std::list<PublicKeyData> & operator()( const Pathname & keyring_r ) const
      { return getData( keyring_r ); }

      void setDirty( const Pathname & keyring_r )
      { _cacheMap[keyring_r].setDirty(); }

    private:
      struct Cache
      {
	Cache() {}

	void setDirty()
	{
	  _keyringK.reset();
	  _keyringP.reset();
	}

	void assertCache( const Pathname & keyring_r )
	{
	  // .kbx since gpg2-2.1
	  if ( !_keyringK )
	    _keyringK.reset( new WatchFile( keyring_r/"pubring.kbx", WatchFile::NO_INIT ) );
	  if ( !_keyringP )
	    _keyringP.reset( new WatchFile( keyring_r/"pubring.gpg", WatchFile::NO_INIT ) );
	}

	bool hasChanged() const
	{
	  bool k = _keyringK->hasChanged();	// be sure both files are checked
	  bool p = _keyringP->hasChanged();
	  return k || p;
	}

	std::list<PublicKeyData> _data;

      private:
	scoped_ptr<WatchFile> _keyringK;
	scoped_ptr<WatchFile> _keyringP;
      };

      typedef std::map<Pathname,Cache> CacheMap;

      const std::list<PublicKeyData> & getData( const Pathname & keyring_r ) const
      {
	Cache & cache( _cacheMap[keyring_r] );
	// init new cache entry
	cache.assertCache( keyring_r );
	return getData( keyring_r, cache );
      }

      const std::list<PublicKeyData> & getData( const Pathname & keyring_r, Cache & cache_r ) const
      {
        if ( cache_r.hasChanged() ) {
          shared_ptr<KeyManagerCtx> ctx = KeyManagerCtx::createForOpenPGP();
          if (ctx) {
            if (ctx->setHomedir(keyring_r)) {
              std::list<PublicKeyData> foundKeys = ctx->listKeys();
              cache_r._data.swap(foundKeys);
            }
          }
          MIL << "Found keys: " << cache_r._data  << endl;
        }
        return cache_r._data;
      }

      mutable CacheMap _cacheMap;
    };
    ///////////////////////////////////////////////////////////////////
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : KeyRing::Impl
  //
  /** KeyRing implementation. */
  struct KeyRing::Impl
  {
    Impl( const Pathname & baseTmpDir )
    : _trusted_tmp_dir( baseTmpDir, "zypp-trusted-kr" )
    , _general_tmp_dir( baseTmpDir, "zypp-general-kr" )
    , _base_dir( baseTmpDir )
    {
      MIL << "Current KeyRing::DefaultAccept: " << _keyRingDefaultAccept << endl;
    }

    void importKey( const PublicKey & key, bool trusted = false );
    void multiKeyImport( const Pathname & keyfile_r, bool trusted_r = false );
    void deleteKey( const std::string & id, bool trusted );

    std::string readSignatureKeyId( const Pathname & signature );

    bool isKeyTrusted( const std::string & id )
    { return bool(publicKeyExists( id, trustedKeyRing() )); }
    bool isKeyKnown( const std::string & id )
    { return publicKeyExists( id, trustedKeyRing() ) || publicKeyExists( id, generalKeyRing() ); }

    std::list<PublicKey> trustedPublicKeys()
    { return publicKeys( trustedKeyRing() ); }
    std::list<PublicKey> publicKeys()
    { return publicKeys( generalKeyRing() ); }

    const std::list<PublicKeyData> & trustedPublicKeyData()
    { return publicKeyData( trustedKeyRing() ); }
    const std::list<PublicKeyData> & publicKeyData()
    { return publicKeyData( generalKeyRing() ); }

    void dumpPublicKey( const std::string & id, bool trusted, std::ostream & stream )
    { dumpPublicKey( id, ( trusted ? trustedKeyRing() : generalKeyRing() ), stream ); }

    PublicKey exportPublicKey( const PublicKeyData & keyData )
    { return exportKey( keyData, generalKeyRing() ); }
    PublicKey exportTrustedPublicKey( const PublicKeyData & keyData )
    { return exportKey( keyData, trustedKeyRing() ); }

    bool verifyFileSignatureWorkflow( const Pathname & file, const std::string & filedesc, const Pathname & signature, bool & sigValid_r, const KeyContext & keycontext = KeyContext());

    bool verifyFileSignature( const Pathname & file, const Pathname & signature )
    { return verifyFile( file, signature, generalKeyRing() ); }
    bool verifyFileTrustedSignature( const Pathname & file, const Pathname & signature )
    { return verifyFile( file, signature, trustedKeyRing() ); }

  private:
    bool verifyFile( const Pathname & file, const Pathname & signature, const Pathname & keyring );
    void importKey( const Pathname & keyfile, const Pathname & keyring );

    PublicKey exportKey( const std::string & id, const Pathname & keyring );
    PublicKey exportKey( const PublicKeyData & keyData, const Pathname & keyring );
    PublicKey exportKey( const PublicKey & key, const Pathname & keyring )
    { return exportKey( key.keyData(), keyring ); }

    void dumpPublicKey( const std::string & id, const Pathname & keyring, std::ostream & stream );
    filesystem::TmpFile dumpPublicKeyToTmp( const std::string & id, const Pathname & keyring );

    void deleteKey( const std::string & id, const Pathname & keyring );

    std::list<PublicKey> publicKeys( const Pathname & keyring);
    const std::list<PublicKeyData> & publicKeyData( const Pathname & keyring )
    { return cachedPublicKeyData( keyring ); }

    /** Get \ref PublicKeyData for ID (\c false if ID is not found). */
    PublicKeyData publicKeyExists( const std::string & id, const Pathname & keyring );

    const Pathname generalKeyRing() const
    { return _general_tmp_dir.path(); }
    const Pathname trustedKeyRing() const
    { return _trusted_tmp_dir.path(); }

    // Used for trusted and untrusted keyrings
    filesystem::TmpDir _trusted_tmp_dir;
    filesystem::TmpDir _general_tmp_dir;
    Pathname _base_dir;

  private:
    /** Functor returning the keyrings data (cached).
     * \code
     *  const std::list<PublicKeyData> & cachedPublicKeyData( const Pathname & keyring );
     * \endcode
     */
    CachedPublicKeyData cachedPublicKeyData;
  };
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
    PublicKeyData ret;
    for ( const PublicKeyData & key : publicKeyData( keyring ) )
    {
      if ( key.providesKey( id ) )
      {
	ret = key;
	break;
      }
    }
    MIL << (ret ? "Found" : "No") << " key [" << id << "] in keyring " << keyring << endl;
    return ret;
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
    KeyManagerCtx::Ptr ctx = KeyManagerCtx::createForOpenPGP();
    if (!ctx || !ctx->setHomedir(keyring))
      return;
    ctx->exportKey(id, stream);
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

  bool KeyRing::Impl::verifyFileSignatureWorkflow( const Pathname & file, const std::string & filedesc, const Pathname & signature, bool & sigValid_r, const KeyContext & context )
  {
    sigValid_r = false;	// set true if signature is actually successfully validated!

    callback::SendReport<KeyRingReport> report;
    MIL << "Going to verify signature for " << filedesc << " ( " << file << " ) with " << signature << endl;

    // if signature does not exists, ask user if he wants to accept unsigned file.
    if( signature.empty() || (!PathInfo( signature ).isExist()) )
    {
      bool res = report->askUserToAcceptUnsignedFile( filedesc, context );
      MIL << "askUserToAcceptUnsignedFile: " << res << endl;
      return res;
    }

    // get the id of the signature (it might be a subkey id!)
    std::string id = readSignatureKeyId( signature );

    // does key exists in trusted keyring
    PublicKeyData trustedKeyData( publicKeyExists( id, trustedKeyRing() ) );
    if ( trustedKeyData )
    {
      MIL << "Key is trusted: " << trustedKeyData << endl;

      // lets look if there is an updated key in the
      // general keyring
      PublicKeyData generalKeyData( publicKeyExists( id, generalKeyRing() ) );
      if ( generalKeyData )
      {
        // bnc #393160: Comment #30: Compare at least the fingerprint
        // in case an attacker created a key the the same id.
	//
	// FIXME: bsc#1008325: For keys using subkeys, we'd actually need
	// to compare the subkey sets, to tell whether a key was updated.
	// because created() remains unchanged if the primary key is not touched.
	// For now we wait until a new subkey signs the data and treat it as a
	//  new key (else part below).
        if ( trustedKeyData.fingerprint() == generalKeyData.fingerprint()
	   && trustedKeyData.created() < generalKeyData.created() )
        {
          MIL << "Key was updated. Saving new version into trusted keyring: " << generalKeyData << endl;
          importKey( exportKey( generalKeyData, generalKeyRing() ), true );
	  trustedKeyData = publicKeyExists( id, trustedKeyRing() ); // re-read: invalidated by import?
	}
      }

      // it exists, is trusted, does it validate?
      report->infoVerify( filedesc, trustedKeyData, context );
      if ( verifyFile( file, signature, trustedKeyRing() ) )
      {
        return (sigValid_r=true);	// signature is actually successfully validated!
      }
      else
      {
	bool res = report->askUserToAcceptVerificationFailed( filedesc, exportKey( trustedKeyData, trustedKeyRing() ), context );
	MIL << "askUserToAcceptVerificationFailed: " << res << endl;
        return res;
      }
    }
    else
    {
      PublicKeyData generalKeyData( publicKeyExists( id, generalKeyRing() ) );
      if ( generalKeyData )
      {
        PublicKey key( exportKey( generalKeyData, generalKeyRing() ) );
        MIL << "Key [" << id << "] " << key.name() << " is not trusted" << endl;

        // ok the key is not trusted, ask the user to trust it or not
        KeyRingReport::KeyTrust reply = report->askUserToAcceptKey( key, context );
        if ( reply == KeyRingReport::KEY_TRUST_TEMPORARILY ||
            reply == KeyRingReport::KEY_TRUST_AND_IMPORT )
        {
          MIL << "User wants to trust key [" << id << "] " << key.name() << endl;

          Pathname whichKeyring;
          if ( reply == KeyRingReport::KEY_TRUST_AND_IMPORT )
          {
            MIL << "User wants to import key [" << id << "] " << key.name() << endl;
            importKey( key, true );
            whichKeyring = trustedKeyRing();
          }
          else
            whichKeyring = generalKeyRing();

          // does it validate?
	  report->infoVerify( filedesc, generalKeyData, context );
          if ( verifyFile( file, signature, whichKeyring ) )
          {
	    return (sigValid_r=true);	// signature is actually successfully validated!
          }
          else
          {
	    bool res = report->askUserToAcceptVerificationFailed( filedesc, key, context );
	    MIL << "askUserToAcceptVerificationFailed: " << res << endl;
	    return res;
          }
        }
        else
        {
          MIL << "User does not want to trust key [" << id << "] " << key.name() << endl;
          return false;
        }
      }
      else
      {
        // signed with an unknown key...
        MIL << "File [" << file << "] ( " << filedesc << " ) signed with unknown key [" << id << "]" << endl;
	bool res = report->askUserToAcceptUnknownKey( filedesc, id, context );
	MIL << "askUserToAcceptUnknownKey: " << res << endl;
	return res;
      }
    }
    return false;
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

    KeyManagerCtx::Ptr ctx = KeyManagerCtx::createForOpenPGP();
    if(!ctx || !ctx->setHomedir(keyring))
      ZYPP_THROW(KeyRingException(_("Failed to import key.")));

    cachedPublicKeyData.setDirty( keyring );
    if(!ctx->importKey(keyfile))
      ZYPP_THROW(KeyRingException(_("Failed to import key.")));
  }

  void KeyRing::Impl::deleteKey( const std::string & id, const Pathname & keyring )
  {
    KeyManagerCtx::Ptr ctx = KeyManagerCtx::createForOpenPGP();
    if(!ctx) {
      ZYPP_THROW(KeyRingException(_("Failed to delete key.")));
    }

    if(!ctx->setHomedir(keyring)) {
      ZYPP_THROW(KeyRingException(_("Failed to delete key.")));
    }

    if(!ctx->deleteKey(id)){
      ZYPP_THROW(KeyRingException(_("Failed to delete key.")));
    }

    cachedPublicKeyData.setDirty( keyring );
  }

  std::string KeyRing::Impl::readSignatureKeyId( const Pathname & signature )
  {
    if ( ! PathInfo( signature ).isFile() )
      ZYPP_THROW(KeyRingException( str::Format(_("Signature file %s not found")) % signature.asString() ));

    MIL << "Determining key id of signature " << signature << endl;

    KeyManagerCtx::Ptr ctx = KeyManagerCtx::createForOpenPGP();
    if(!ctx) {
      return std::string();
    }

    std::list<std::string> fprs = ctx->readSignatureFingerprints(signature);
    if (fprs.size()) {
      std::string &id = fprs.back();
      MIL << "Determined key id [" << id << "] for signature " << signature << endl;
      return id;
    }
    return std::string();
  }

  bool KeyRing::Impl::verifyFile( const Pathname & file, const Pathname & signature, const Pathname & keyring )
  {
    KeyManagerCtx::Ptr ctx = KeyManagerCtx::createForOpenPGP();
    if (!ctx || !ctx->setHomedir(keyring))
      return false;

    return ctx->verify(file, signature);
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

  bool KeyRing::verifyFileSignatureWorkflow( const Pathname & file, const std::string & filedesc, const Pathname & signature, bool & sigValid_r, const KeyContext & keycontext )
  { return _pimpl->verifyFileSignatureWorkflow( file, filedesc, signature, sigValid_r, keycontext ); }

  bool KeyRing::verifyFileSignatureWorkflow( const Pathname & file, const std::string filedesc, const Pathname & signature, const KeyContext & keycontext )
  { bool unused; return _pimpl->verifyFileSignatureWorkflow( file, filedesc, signature, unused, keycontext ); }

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
