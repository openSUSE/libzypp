/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/zypp_detail/repomanagerbase_p.h
 *
*/
#ifndef ZYPP_ZYPP_DETAIL_KEYRINGIMPL_H
#define ZYPP_ZYPP_DETAIL_KEYRINGIMPL_H

#include "zypp-core/fs/TmpPath.h"
#include <zypp-core/fs/WatchFile>
#include <zypp/KeyManager.h>
#include <zypp/KeyRing.h>

#include <optional>

namespace zypp {


  ///////////////////////////////////////////////////////////////////
  /// \class CachedPublicKeyData
  /// \brief Functor returning the keyrings data (cached).
  /// \code
  ///   const std::list<PublicKeyData> & cachedPublicKeyData( const Pathname & keyring );
  /// \endcode
  ///////////////////////////////////////////////////////////////////
  struct CachedPublicKeyData : private base::NonCopyable
  {
    const std::list<PublicKeyData> & operator()( const Pathname & keyring_r ) const;

    void setDirty( const Pathname & keyring_r );

    ///////////////////////////////////////////////////////////////////
    /// \brief Helper providing on demand a KeyManagerCtx to manip the cached keyring.
    ///
    /// The 1st call to \ref keyManagerCtx creates the \ref KeyManagerCtx. Returning
    /// the context tags the cached data as dirty. Should be used to import/delete keys
    /// in a cache keyring.
    struct Manip {
      NON_COPYABLE_BUT_MOVE( Manip );
      Manip( CachedPublicKeyData & cache_r, Pathname keyring_r );

      KeyManagerCtx & keyManagerCtx();
    private:
      CachedPublicKeyData & _cache;
      Pathname _keyring;
      std::optional<KeyManagerCtx> _context;
    };
    ///////////////////////////////////////////////////////////////////

    /** Helper providing on demand a KeyManagerCtx to manip the cached keyring. */
    Manip manip( Pathname keyring_r );

  private:
    struct Cache
    {
      Cache();

      void setDirty();

      void assertCache( const Pathname & keyring_r );

      bool hasChanged() const;

      std::list<PublicKeyData> _data;

    private:

      scoped_ptr<WatchFile> _keyringK;
      scoped_ptr<WatchFile> _keyringP;
    };

    typedef std::map<Pathname,Cache> CacheMap;

    const std::list<PublicKeyData> & getData( const Pathname & keyring_r ) const;

    const std::list<PublicKeyData> & getData( const Pathname & keyring_r, Cache & cache_r ) const;

    mutable CacheMap _cacheMap;
  };



  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : KeyRing::Impl
  //
  /** KeyRing implementation. */
  struct KeyRing::Impl
  {
    Impl( const Pathname & baseTmpDir );

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

    bool verifyFileSignature( const Pathname & file, const Pathname & signature )
    { return verifyFile( file, signature, generalKeyRing() ); }
    bool verifyFileTrustedSignature( const Pathname & file, const Pathname & signature )
    { return verifyFile( file, signature, trustedKeyRing() ); }

    PublicKeyData publicKeyExists( const std::string & id )
    { return publicKeyExists(id, generalKeyRing());}
    PublicKeyData trustedPublicKeyExists( const std::string & id )
    { return publicKeyExists(id, trustedKeyRing());}

    void allowPreload( bool yesno_r )
    { _allowPreload = yesno_r; }

    /** Impl helper providing on demand a KeyManagerCtx to manip a cached keyring. */
    CachedPublicKeyData::Manip keyRingManip( const Pathname & keyring )
    { return cachedPublicKeyData.manip( keyring ); }

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
    /** Load key files cached on the system into the generalKeyRing. */
    void preloadCachedKeys();

    const Pathname generalKeyRing() const
    { return _general_tmp_dir.path(); }
    const Pathname trustedKeyRing() const
    { return _trusted_tmp_dir.path(); }

  private:
    // Used for trusted and untrusted keyrings
    filesystem::TmpDir _trusted_tmp_dir;
    filesystem::TmpDir _general_tmp_dir;
    Pathname _base_dir;
    bool _allowPreload = false;	//< General keyring may be preloaded with keys cached on the system.

    /** Functor returning the keyrings data (cached).
     * \code
     *  const std::list<PublicKeyData> & cachedPublicKeyData( const Pathname & keyring );
     * \endcode
     */
    CachedPublicKeyData cachedPublicKeyData;
  };

}


#endif
