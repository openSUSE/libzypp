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
      // version of the key adds nothing and a later renewal does.
      ByteArray before, after;
      if ( ! ctx.exportKey( key_r.id(), before ) )
      {
        ERR << "No copy of " << key_r << " to merge with" << endl;
        return std::nullopt;
      }
      if ( ! ctx.importKey( key_r.path() ) || ! ctx.exportKey( key_r.id(), after ) )
      {
        ERR << "Failed to merge " << key_r << endl;
        return std::nullopt;
      }

      // Keys are found by their long id. It must have been this very key we exported.
      for ( const PublicKeyData & keyData : ctx.listKeys() )
      {
        if ( keyData.id() == key_r.id() && str::compareCI( keyData.fingerprint(), fpr ) != 0 )
        {
          ERR << "Not merging " << key_r << ": " << keyData << " has the same id" << endl;
          return std::nullopt;
        }
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
