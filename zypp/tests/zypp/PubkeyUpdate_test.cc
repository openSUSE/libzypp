#include <fstream>

#include <boost/test/unit_test.hpp>

#include <zypp/TmpPath.h>
#include <zypp/target/private/pubkeyupdate_p.h>

#include "KeyRenewalFixture.h"

using namespace zypp;
using namespace keyrenewal;
using target::InstalledPubkey;

namespace
{
  PublicKey load( const char * name_r )
  { return PublicKey( dataDir()/name_r ); }

  PublicKey loadRing( const char * name_r )
  { return PublicKey( Pathname( TESTS_SRC_DIR )/"zypp/data/KeyRing"/name_r ); }

  // The key merged for rpm, as a PublicKey.
  PublicKey keyOf( const std::optional<ByteArray> & merged_r )
  {
    BOOST_REQUIRE( merged_r );
    filesystem::TmpFile file;
    std::ofstream( file.path().c_str() ).write( merged_r->data(), merged_r->size() );
    return PublicKey( file );
  }

  Date subkeyExpires( const PublicKey & key_r )
  {
    BOOST_REQUIRE( key_r.hasSubkeys() );
    return (*key_r.subkeys().begin()).expires();
  }

  // What rpm needs to import to be up to date, if anything: rpm holds \a inRpm_r, \a key_r is the new key.
  std::optional<ByteArray> merged( const std::vector<PublicKey> & inRpm_r, const PublicKey & key_r )
  {
    std::vector<InstalledPubkey> copies;
    for ( const PublicKey & key : inRpm_r )
      copies.push_back( InstalledPubkey { Edition( key.gpgPubkeyVersion(), "1" ), key } );
    return target::mergedKeyUpdate( copies, key_r );
  }

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

// A renewal of the subkey only: rpm's release is not the one zypp computes, so releases can not tell.
BOOST_AUTO_TEST_CASE(merged_update_subkey_renewal)
{
  const PublicKey r1( load( "sub-r1.asc" ) ), r2( load( "sub-r2.asc" ) );
  BOOST_REQUIRE_EQUAL( r1.fingerprint(), r2.fingerprint() );
  BOOST_REQUIRE_EQUAL( r1.gpgPubkeyRelease(), r2.gpgPubkeyRelease() );

  const PublicKey update { keyOf( merged( { r1 }, r2 ) ) };
  BOOST_CHECK_EQUAL( update.fingerprint(), r2.fingerprint() );
  BOOST_CHECK_EQUAL( subkeyExpires( update ), subkeyExpires( r2 ) );
  BOOST_CHECK( subkeyExpires( update ) > subkeyExpires( r1 ) );
}

BOOST_AUTO_TEST_CASE(merged_update_primary_key_renewal)
{
  const PublicKey oldKey( loadRing( "renew-old.asc" ) ), newKey( loadRing( "renew-new.asc" ) );
  const PublicKey update { keyOf( merged( { oldKey }, newKey ) ) };
  BOOST_CHECK_EQUAL( update.fingerprint(), newKey.fingerprint() );
  BOOST_CHECK_EQUAL( update.created(), newKey.created() );
  BOOST_CHECK_EQUAL( update.expires(), newKey.expires() );
}

// An older version of a key, or the same one, adds nothing. Importing an older version into gpg
// keeps its older self-signature, which the merged key must not show.
BOOST_AUTO_TEST_CASE(merged_update_adds_nothing)
{
  const PublicKey r1( load( "sub-r1.asc" ) ), r2( load( "sub-r2.asc" ) );
  BOOST_CHECK( ! merged( { r2 }, r1 ) );	// older
  BOOST_CHECK( ! merged( { r2 }, r2 ) );	// same
  BOOST_CHECK( ! merged( { r1 }, r1 ) );

  const PublicKey oldKey( loadRing( "renew-old.asc" ) ), newKey( loadRing( "renew-new.asc" ) );
  BOOST_CHECK( ! merged( { newKey }, oldKey ) );
  BOOST_CHECK( ! merged( { newKey }, newKey ) );

  // all copies in rpm count: nothing to update if one of them has it all
  BOOST_CHECK( ! merged( { r1, r2 }, r1 ) );
  BOOST_CHECK( ! merged( { r1, r2 }, r2 ) );
}

// Data that the key did not sign itself is not merged: gpg drops it.
BOOST_AUTO_TEST_CASE(merged_update_ignores_invalid_data)
{
  const PublicKey r1( load( "sub-r1.asc" ) ), v1( load( "sub-v1.asc" ) );
  BOOST_REQUIRE_EQUAL( r1.fingerprint(), load( "sub-r2-badbinding.asc" ).fingerprint() );
  BOOST_CHECK( ! merged( { r1 }, load( "sub-r2-badbinding.asc" ) ) );	// invalid binding signature of the renewal
  BOOST_CHECK( ! merged( { v1 }, load( "sub-foreign.asc" ) ) );		// subkey of an unrelated key
}

// Only copies of the same key are merged.
BOOST_AUTO_TEST_CASE(merged_update_other_keys)
{
  const PublicKey r1( load( "sub-r1.asc" ) ), r2( load( "sub-r2.asc" ) ), keyV0( load( "key-v0.asc" ) );
  BOOST_REQUIRE_NE( r1.fingerprint(), keyV0.fingerprint() );

  BOOST_CHECK( merged( { keyV0, r1 }, r2 ) );	// the other key is ignored
  BOOST_CHECK( ! merged( { keyV0 }, r2 ) );	// no copy of this key to update
  BOOST_CHECK( ! merged( {}, r2 ) );
}
