#include <boost/test/unit_test.hpp>

#include <zypp/target/private/pubkeyupdate_p.h>

#include "KeyRenewalFixture.h"

using namespace zypp;
using namespace keyrenewal;
using target::InstalledPubkey;

namespace
{
  PublicKey load( const char * name_r )
  { return PublicKey( dataDir()/name_r ); }

  // as rpm holds it: named by the version of the key and some release
  InstalledPubkey installed( const PublicKey & named_r, const char * release_r, const PublicKey & holds_r )
  { return InstalledPubkey { Edition( named_r.gpgPubkeyVersion(), release_r ), holds_r }; }
}

BOOST_AUTO_TEST_CASE(pubkey_copies_same_key)
{
  const PublicKey subV0( load( "sub-v0.asc" ) ), subV1( load( "sub-v1.asc" ) ), keyV0( load( "key-v0.asc" ) );
  BOOST_REQUIRE_EQUAL( subV0.fingerprint(), subV1.fingerprint() );
  BOOST_REQUIRE_NE( subV0.gpgPubkeyVersion(), keyV0.gpgPubkeyVersion() );

  const std::vector<InstalledPubkey> inRpm {
    installed( subV0, "6812b980", subV0 ),
    installed( keyV0, "67748580", keyV0 ),
  };

  // Only the copy of the same key; packages with other versions are not listed at all.
  const target::PubkeyCopies copies { target::findPubkeyCopies( inRpm, subV1 ) };
  BOOST_REQUIRE_EQUAL( copies.sameKey.size(), 1 );
  BOOST_CHECK_EQUAL( copies.sameKey.front().edition, Edition( subV0.gpgPubkeyVersion(), "6812b980" ) );
  BOOST_CHECK( copies.others.empty() );

  // Several releases of the same key
  const std::vector<InstalledPubkey> several {
    installed( subV0, "67748580", subV0 ),
    installed( subV0, "6812b980", subV0 ),
  };
  BOOST_CHECK_EQUAL( target::findPubkeyCopies( several, subV1 ).sameKey.size(), 2 );

  // Nothing installed
  const target::PubkeyCopies none { target::findPubkeyCopies( {}, subV1 ) };
  BOOST_CHECK( none.sameKey.empty() && none.others.empty() );
}

BOOST_AUTO_TEST_CASE(pubkey_copies_same_version_different_key)
{
  const PublicKey subV0( load( "sub-v0.asc" ) ), subV1( load( "sub-v1.asc" ) ), keyV0( load( "key-v0.asc" ) );

  // An installed package named with the version of subV1, that holds another key.
  const std::vector<InstalledPubkey> inRpm {
    installed( subV1, "6812b980", keyV0 ),
    installed( subV1, "67748580", subV0 ),
  };

  const target::PubkeyCopies copies { target::findPubkeyCopies( inRpm, subV1 ) };
  BOOST_REQUIRE_EQUAL( copies.sameKey.size(), 1 );
  BOOST_CHECK_EQUAL( copies.sameKey.front().edition, Edition( subV1.gpgPubkeyVersion(), "67748580" ) );
  BOOST_REQUIRE_EQUAL( copies.others.size(), 1 );
  BOOST_CHECK_EQUAL( copies.others.front().edition, Edition( subV1.gpgPubkeyVersion(), "6812b980" ) );
}
