#include <boost/test/unit_test.hpp>

#include <zypp-common/KeyManager.h>
#include <zypp-common/PublicKey.h>

#include "KeyRenewalFixture.h"

using namespace zypp;
using namespace keyrenewal;

namespace
{
  /** The keys in \a keydata, read in a keyring of their own. */
  std::list<PublicKeyData> keysIn( const ByteArray & keydata_r )
  {
    KeyManagerCtx ctx { KeyManagerCtx::createForOpenPGP() };
    BOOST_REQUIRE( ctx.importKey( keydata_r ) );
    return ctx.listKeys();
  }
}

// A key id may be shared by different keys, a fingerprint identifies a key. The two lookups are
// kept apart: exportKey finds by id only, exportKeyByFpr by fingerprint only.
BOOST_AUTO_TEST_CASE(export_by_id_and_by_fingerprint)
{
  const PublicKey keyV0( dataDir()/"key-v0.asc" ), subV0( dataDir()/"sub-v0.asc" );
  BOOST_REQUIRE_NE( keyV0.fingerprint(), subV0.fingerprint() );

  KeyManagerCtx ctx { KeyManagerCtx::createForOpenPGP() };
  BOOST_REQUIRE( ctx.importKey( keyV0.path() ) );
  BOOST_REQUIRE( ctx.importKey( subV0.path() ) );
  BOOST_REQUIRE_EQUAL( ctx.listKeys().size(), 2 );

  // By fingerprint: exactly this key, case does not matter.
  for ( const std::string & fpr : { subV0.fingerprint(), str::toLower( subV0.fingerprint() ) } )
  {
    ByteArray keydata;
    BOOST_REQUIRE( ctx.exportKeyByFpr( fpr, keydata ) );
    const std::list<PublicKeyData> keys { keysIn( keydata ) };
    BOOST_REQUIRE_EQUAL( keys.size(), 1 );
    BOOST_CHECK_EQUAL( keys.front().fingerprint(), subV0.fingerprint() );
  }

  // By id: the same key.
  {
    ByteArray keydata;
    BOOST_REQUIRE( ctx.exportKey( subV0.id(), keydata ) );
    const std::list<PublicKeyData> keys { keysIn( keydata ) };
    BOOST_REQUIRE_EQUAL( keys.size(), 1 );
    BOOST_CHECK_EQUAL( keys.front().fingerprint(), subV0.fingerprint() );
  }

  // The lookups don't mix: an id is not a fingerprint, a fingerprint is not an id.
  ByteArray keydata;
  BOOST_CHECK( ! ctx.exportKeyByFpr( subV0.id(), keydata ) );
  BOOST_CHECK( ! ctx.exportKey( subV0.fingerprint(), keydata ) );

  // Unknown fingerprint.
  BOOST_CHECK( ! ctx.exportKeyByFpr( std::string( 40, '0' ), keydata ) );
}
