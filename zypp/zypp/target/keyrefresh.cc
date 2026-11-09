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

#include <zypp/ZYppFactory.h>
#include <zypp/KeyRing.h>
#include <zypp/target/rpm/RpmHeader.h>
#include <zypp/target/private/keyrefresh_p.h>

#include <zypp/ng/context.h>
#include <zypp/ng/workflows/repoinfowf.h>

using std::endl;

namespace zypp::target
{
  bool keyNeedsRefresh( const PublicKeyData & key_r, const std::string & signingKeyId_r, Date now_r )
  {
    if ( not key_r )
      return false;

    // Not PublicKeyData::expired()/daysToLive(): they always use the current time.
    auto needsRefresh = [now_r]( Date::ValueType expires_r ) -> bool {
      return expires_r != 0	// 0: never expires
          && expires_r - Date::ValueType( now_r ) < 15 * Date::day;
    };

    if ( needsRefresh( key_r.expires() ) )
      return true;

    // rpm also refuses a signature made by an expired signing subkey, even if the primary key is fine.
    if ( not signingKeyId_r.empty() ) {
      for ( const PublicSubkeyData & sub : key_r.subkeys() ) {
        if ( str::endsWithCI( sub.id(), signingKeyId_r ) && needsRefresh( sub.expires() ) )
          return true;
      }
    }
    return false;
  }

  bool KeyRefreshTracker::firstTime( const std::string & keyId_r, const std::string & repoAlias_r )
  { return _seen.insert( { str::toUpper( keyId_r ), repoAlias_r } ).second; }

  /** bsc#1289557: Renew an expiring trusted key before rpm installs a package signed with it.
   *
   * For repos with validated metadata zypp does not check the package signature itself
   * (checksum from the signed metadata), but rpm does when installing. If the key in the
   * rpm database expired and the repo meanwhile provides a renewed key, fetch the repo keys;
   * a renewed trusted key imported into the general keyring is passed on to rpm.
   * Never throws: this must not break the commit.
   */
  void refreshExpiringKey( const PoolItem & pi_r, const ManagedFile & file_r, KeyRefreshTracker & tracker_r )
  {
    try
    {
      if ( file_r->empty() )
        return;

      // No verification: it must not fail on exactly the expired key.
      rpm::RpmHeader::constPtr hdr { rpm::RpmHeader::readPackage( file_r.value(), rpm::RpmHeader::NOVERIFY ) };
      if ( ! hdr )
        return;

      const std::string keyId { hdr->signatureKeyID() };
      if ( keyId.empty() )
        return;

      // Unknown keys are the job of the CHK_NOKEY handling in PackageProvider.
      const PublicKeyData keyData { getZYpp()->keyRing()->trustedPublicKeyData( keyId ) };
      if ( ! keyData || ! keyNeedsRefresh( keyData, keyId ) )
        return;

      const RepoInfo repo { pi_r.repoInfo() };
      if ( ! tracker_r.firstTime( keyData.id(), repo.alias() ) )
        return;

      MIL << "Trusted key " << keyData << " signing " << pi_r
          << " is expired or about to expire. Looking for a renewed key in repo '" << repo.alias() << "'" << endl;

      const auto res { zyppng::RepoInfoWorkflow::fetchGpgKeys( zyppng::Context::defaultContext(), repo ) };
      if ( ! res )
      {
        try { std::rethrow_exception( res.error() ); }
        catch ( const Exception & excpt )
        { ZYPP_CAUGHT( excpt ); WAR << "Failed to fetch keys from repo '" << repo.alias() << "': " << excpt.asString() << endl; }
        catch ( ... )
        { WAR << "Failed to fetch keys from repo '" << repo.alias() << "'" << endl; }
      }
    }
    catch ( const Exception & excpt )
    { ZYPP_CAUGHT( excpt ); WAR << "Failed to refresh key for " << pi_r << ": " << excpt.asString() << endl; }
    catch ( ... )
    { WAR << "Failed to refresh key for " << pi_r << endl; }
  }
} // namespace zypp::target
