/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_TARGET_PRIVATE_PUBKEYUPDATE_P_H
#define ZYPP_TARGET_PRIVATE_PUBKEYUPDATE_P_H

#include <optional>
#include <vector>

#include <zypp-core/ByteArray.h>
#include <zypp/Edition.h>
#include <zypp-common/PublicKey.h>

namespace zypp::target
{
  /** A gpg-pubkey package in the rpm database: how rpm names it, and the key it holds. */
  struct InstalledPubkey
  {
    Edition edition;
    PublicKey key;
  };

  /** The installed gpg-pubkey packages that matter when a key is replaced in rpm. */
  struct PubkeyCopies
  {
    std::vector<InstalledPubkey> sameKey;	///< Hold the key with the same fingerprint as the new key.
    std::vector<InstalledPubkey> others;	///< Have the same version (short id), but hold a different key.
  };

  /** Sort the \a installed_r packages with the version (short id) of \a key_r by the key they hold.
   *
   * The version of a gpg-pubkey is only the short id of the key, the fingerprint is what identifies it.
   * Packages with another version are not listed at all.
   */
  PubkeyCopies findPubkeyCopies( const std::vector<InstalledPubkey> & installed_r, const PublicKey & key_r );

  /** The key to import into rpm to update its copies \a copies_r of the key \a key_r, or nothing if
   * \a key_r adds nothing to them.
   *
   * gpg merges all of them in a temporary keyring and the merged key is returned. Like in the zypp
   * keyrings, gpg only keeps data the primary key validly signed, and an older version of a key adds
   * nothing. Only keys with the fingerprint of \a key_r are considered.
   *
   * If gpg fails the error is logged and nothing is returned: rpm is left alone.
   */
  std::optional<ByteArray> mergedKeyUpdate( const std::vector<InstalledPubkey> & copies_r, const PublicKey & key_r );
}

#endif
