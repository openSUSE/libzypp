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

  namespace
  {
    /** Renew an expiring trusted key from \a repo_r, if the package \a file_r is signed with it.
     *
     * Reads the signing key id from the package, and if the trusted key (or the signing subkey)
     * is expired or about to expire, fetches the repo keys. A renewed trusted key imported into
     * the general keyring is passed on to rpm. If \a tracker_r is given, this is tried only once
     * per (key id, repo).
     *
     * \return Whether the key was expiring and a fetch of the repo keys was tried, i.e. checking the
     * signature again is worthwhile. It does not tell whether a renewed key was found.
     * Never throws: this must not break the commit or the package check.
     */
    bool renewExpiringKey( const RepoInfo & repo_r, const Pathname & file_r, KeyRefreshTracker * tracker_r )
    {
      try
      {
        if ( file_r.empty() )
          return false;

        // No verification: it must not fail on exactly the expired key.
        rpm::RpmHeader::constPtr hdr { rpm::RpmHeader::readPackage( file_r, rpm::RpmHeader::NOVERIFY ) };
        if ( ! hdr )
          return false;

        const std::string keyId { hdr->signatureKeyID() };
        if ( keyId.empty() )
          return false;

        // Unknown keys are the job of the CHK_NOKEY handling in PackageProvider.
        const PublicKeyData keyData { getZYpp()->keyRing()->trustedPublicKeyData( keyId ) };
        if ( ! keyData || ! keyNeedsRefresh( keyData, keyId ) )
          return false;

        if ( tracker_r && ! tracker_r->firstTime( keyData.id(), repo_r.alias() ) )
          return false;

        MIL << "Trusted key " << keyData << " signing " << file_r
            << " is expired or about to expire. Looking for a renewed key in repo '" << repo_r.alias() << "'" << endl;

        const auto res { zyppng::RepoInfoWorkflow::fetchGpgKeys( zyppng::Context::defaultContext(), repo_r ) };
        if ( ! res )
        {
          try { std::rethrow_exception( res.error() ); }
          catch ( const Exception & excpt )
          { ZYPP_CAUGHT( excpt ); WAR << "Failed to fetch keys from repo '" << repo_r.alias() << "': " << excpt.asString() << endl; }
          catch ( ... )
          { WAR << "Failed to fetch keys from repo '" << repo_r.alias() << "'" << endl; }
          return false;
        }
        return true;
      }
      catch ( const Exception & excpt )
      { ZYPP_CAUGHT( excpt ); WAR << "Failed to refresh key for " << file_r << ": " << excpt.asString() << endl; }
      catch ( ... )
      { WAR << "Failed to refresh key for " << file_r << endl; }
      return false;
    }
  } // namespace

  void refreshExpiringKey( const PoolItem & pi_r, const ManagedFile & file_r, KeyRefreshTracker & tracker_r )
  { renewExpiringKey( pi_r.repoInfo(), file_r.value(), &tracker_r ); }

  bool renewExpiringSigningKey( const RepoInfo & repo_r, const Pathname & file_r )
  { return renewExpiringKey( repo_r, file_r, nullptr ); }
} // namespace zypp::target
