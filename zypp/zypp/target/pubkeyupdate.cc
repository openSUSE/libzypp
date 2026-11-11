/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include <zypp-core/base/Logger.h>
#include <zypp-core/base/Exception.h>
#include <zypp-core/base/String.h>
#include <zypp-common/KeyManager.h>

#include <zypp/target/private/pubkeyupdate_p.h>

using std::endl;

namespace zypp::target
{
  PubkeyCopies findPubkeyCopies( const std::vector<InstalledPubkey> & installed_r, const PublicKey & key_r )
  {
    PubkeyCopies ret;
    // rpm 4 names a gpg-pubkey by the short key id (gpgPubkeyVersion). rpm 6 uses the full
    // fingerprint as version instead: when moving to rpm 6, compare the version with the
    // fingerprint here (and in RpmDb's installedPubkeys).
    const std::string version { key_r.gpgPubkeyVersion() };
    for ( const InstalledPubkey & installed : installed_r )
    {
      if ( installed.edition.version() != version )
        continue;
      if ( str::compareCI( installed.key.fingerprint(), key_r.fingerprint() ) == 0 )
        ret.sameKey.push_back( installed );
      else
        ret.others.push_back( installed );
    }
    return ret;
  }

  std::optional<ByteArray> mergedKeyUpdate( const std::vector<InstalledPubkey> & copies_r, const PublicKey & key_r )
  {
    try
    {
      KeyManagerCtx ctx { KeyManagerCtx::createForOpenPGP() };	// volatile keyring
      const std::string fpr { key_r.fingerprint() };

      for ( const InstalledPubkey & copy : copies_r )
      {
        if ( str::compareCI( copy.key.fingerprint(), fpr ) != 0 )
        {
          WAR << "Ignoring " << copy.key << ": not the key " << key_r << endl;
          continue;
        }
        if ( ! ctx.importKey( copy.key.path() ) )
        {
          ERR << "Failed to import " << copy.key << " into the temporary keyring" << endl;
          return std::nullopt;
        }
      }

      // The export is the minimal one (only the newest self-signatures), so an older
      // version of the key adds nothing and a later renewal does. Exported by fingerprint:
      // the file of key_r may contain other keys, which may even have the same id.
      ByteArray before, after;
      if ( ! ctx.exportKeyByFpr( fpr, before ) )
      {
        ERR << "No copy of " << key_r << " to merge with" << endl;
        return std::nullopt;
      }
      if ( ! ctx.importKey( key_r.path() ) || ! ctx.exportKeyByFpr( fpr, after ) )
      {
        ERR << "Failed to merge " << key_r << endl;
        return std::nullopt;
      }

      if ( before == after )
        return std::nullopt;
      return after;
    }
    catch ( const Exception & excpt )
    {
      ZYPP_CAUGHT( excpt );
      ERR << "Failed to merge " << key_r << ": " << excpt.asString() << endl;
    }
    return std::nullopt;
  }
}
