/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include <zypp-core/base/String.h>

#include <zypp/target/private/pubkeyupdate_p.h>

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
}
