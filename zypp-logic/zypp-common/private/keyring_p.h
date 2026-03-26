/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp-common/private/keyring_p.h
 *
*/
#ifndef ZYPP_PRIVATE_KEYRINGIMPL_H
#define ZYPP_PRIVATE_KEYRINGIMPL_H

#include <zypp-core/ng/base/signals.h>
#include <zypp-core/fs/TmpPath.h>
#include <zypp-core/fs/WatchFile>
#include <zypp-common/KeyManager.h>
#include <zypp-core/ng/pipelines/expected.h>
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

    using CacheMap = std::map<Pathname, Cache>;

    const std::list<PublicKeyData> & getData( const Pathname & keyring_r ) const;

    const std::list<PublicKeyData> & getData( const Pathname & keyring_r, Cache & cache_r ) const;

    mutable CacheMap _cacheMap;
  };

  /**
   * @brief KeyRing implementation, shared between zyppng and zypp
   */
  class KeyRingImpl
  {
    public:
      enum class Ring : bool {
        General = false,
        Trusted = true,
      };

      zyppng::SignalProxy<void ( const PublicKey &/*key*/ )> sigTrustedKeyAdded()
      { return _sigTrustedKeyAdded; }

      zyppng::SignalProxy<void ( const PublicKey &/*key*/ )> sigTrustedKeyRemoved()
      { return _sigTrustedKeyRemoved; }

    public:
      KeyRingImpl( const Pathname & baseTmpDir );

    public:
      void allowPreload( bool yesno_r ) //< General keyring may be preloaded with keys cached on the system.
      { _allowPreload = yesno_r; }

      /** Import PublicKeys into a Ring.
       * Beware that a \ref PublicKey constructed from a file may in fact hold
       * multiple keys (\see \ref PublicKey::hiddenKeys)
       */
      void importKey ( const PublicKey & key, const Ring ring );
      void importKeys( const std::list<PublicKey> & keys, const Ring ring );

      PublicKey exportKey( const std::string & id, const Ring ring ) const
      { return exportKey( id, keyRingPath( ring ) ); }
      PublicKey exportKey( const PublicKeyData & keyData, const Ring ring ) const
      { return exportKey( keyData, keyRingPath( ring ) ); }
      PublicKey exportKey( const PublicKey & key, const Ring ring ) const
      { return exportKey( key.keyData(), ring ); }

      void deleteKey( const std::string & id, const Ring ring );

      void dumpPublicKey( const std::string & id, const Ring ring, std::ostream & stream )
      { dumpPublicKey( id, keyRingPath( ring ), stream ); }

      bool verifyFile( const Pathname & file, const Pathname & signature, const Ring ring )
      { return verifyFile( file, signature, keyRingPath( ring ) ); }

      std::string readSignatureKeyId( const Pathname & signature );

      /** Used by RpmDB to import the trusted keys.
       * Use with care - it's a silent import of all keys. No extra logging.
       * Wrap \a keyfile_r into a \ref PublicKey and call \ref importKey to
       * get the same result plus the log lines.
       */
      void multiKeyImport( const Pathname & keyfile_r, const Ring ring );

    public:
      bool isKeyTrusted( const std::string & id ) const
      { return bool(publicKeyData( id, Ring::Trusted )); }

      bool isKeyKnown( const std::string & id ) const
      { return publicKeyData( id, Ring::Trusted ) || publicKeyData( id, Ring::General ); }

      PublicKeyData publicKeyData( const std::string & id, const Ring ring ) const
      { return publicKeyData( id, keyRingPath( ring ) ); }
      PublicKeyData publicKeyData( const PublicKeyData & keyData, const Ring ring ) const
      { return publicKeyData( keyData.id(), ring ); }
      PublicKeyData publicKeyData( const PublicKey & key, const Ring ring ) const
      { return publicKeyData( key.keyData().id(), ring ); }

      const std::list<PublicKeyData> & publicKeyData( const Ring ring ) const
      { return publicKeyData( keyRingPath( ring ) ); }

      std::list<PublicKey> publicKeys( const Ring ring ) const
      { return publicKeys( keyRingPath( ring ) ); }


    private:
      const Pathname keyRingPath( const Ring ring ) const
      { return ring == Ring::General ? _general_tmp_dir.path() : _trusted_tmp_dir.path(); }

      void importKey( const Pathname & keyfile, const Pathname & keyring );

      PublicKey exportKey( const std::string & id, const Pathname & keyring ) const;
      PublicKey exportKey( const PublicKeyData & keyData, const Pathname & keyring ) const;

      void deleteKey( const std::string & id, const Pathname & keyring );

      PublicKeyData publicKeyData( const std::string & id, const Pathname & keyring ) const;

      const std::list<PublicKeyData> & publicKeyData( const Pathname & keyring ) const
      { return cachedPublicKeyData( keyring ); }

      std::list<PublicKey> publicKeys( const Pathname & keyring ) const;

      void dumpPublicKey( const std::string & id, const Pathname & keyring, std::ostream & stream ) const;
      filesystem::TmpFile dumpPublicKeyToTmp( const std::string & id, const Pathname & keyring ) const;

      bool verifyFile( const Pathname & file, const Pathname & signature, const Pathname & keyring );

    private:
      /** Load key files cached on the system into the generalKeyRing. */
      void preloadCachedKeys();

      /** Impl helper providing on demand a KeyManagerCtx to manip a cached keyring. */
      CachedPublicKeyData::Manip keyRingManip( const Pathname & keyring )
      { return cachedPublicKeyData.manip( keyring ); }

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

      zyppng::Signal<void ( const PublicKey &/*key*/ )> _sigTrustedKeyAdded;
      zyppng::Signal<void ( const PublicKey &/*key*/ )> _sigTrustedKeyRemoved;
  };

  inline std::ostream & operator<<( std::ostream & str, KeyRingImpl::Ring ring )
  { return str << ( ring == KeyRingImpl::Ring::General ? "generalKeyRing" : "trustedKeyRing" ); }
}


#endif
