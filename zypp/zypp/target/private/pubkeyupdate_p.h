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

#include <vector>

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
}

#endif
