/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_TARGET_PRIVATE_KEYREFRESH_P_H
#define ZYPP_TARGET_PRIVATE_KEYREFRESH_P_H

#include <set>
#include <string>
#include <utility>

#include <zypp-core/Date.h>
#include <zypp-core/ManagedFile.h>
#include <zypp-common/PublicKey.h>
#include <zypp/PoolItem.h>

namespace zypp::target
{
  /** Whether a trusted key should be refreshed from its repo before it is used to verify a signature.
   *
   * True if the key is expired or expires in less than 15 days (the same limit
   * zypper uses to warn about expiring keys). Keys that never expire (\ref PublicKeyData::expires
   * is \c 0) and empty keys are never refreshed.
   *
   * \a signingKeyId_r is the id of the key that made the signature. If it is a subkey, its
   * expiry counts as well, because rpm refuses signatures of an expired subkey even if the
   * primary key is still valid.
   *
   * \a now_r is a parameter, so tests do not depend on the current date.
   */
  bool keyNeedsRefresh( const PublicKeyData & key_r, const std::string & signingKeyId_r, Date now_r = Date::now() );

  /** Remembers which (key id, repo alias) pairs were already tried in one commit. */
  class KeyRefreshTracker
  {
  public:
    /** True on the first call for this pair, false afterwards. The key id is case insensitive. */
    bool firstTime( const std::string & keyId_r, const std::string & repoAlias_r );

  private:
    std::set<std::pair<std::string,std::string>> _seen;
  };

  /** bsc#1289557: Renew an expiring trusted key before rpm installs a package signed with it.
   *
   * Reads the signing key id from \a file_r. If the trusted key needs a refresh (\ref keyNeedsRefresh)
   * and the (key id, repo) pair was not yet tried (\a tracker_r), fetches the repo keys. A renewed
   * key imported into the general keyring is passed on to rpm. Never throws.
   */
  void refreshExpiringKey( const PoolItem & pi_r, const ManagedFile & file_r, KeyRefreshTracker & tracker_r );
} // namespace zypp::target

#endif // ZYPP_TARGET_PRIVATE_KEYREFRESH_P_H
