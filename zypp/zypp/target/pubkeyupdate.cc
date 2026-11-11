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
