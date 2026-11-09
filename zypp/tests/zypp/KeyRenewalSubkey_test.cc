// bsc#1284494, bsc#1289557: The signing subkey of a trusted key expired and the vendor renewed it.
// The primary key never expires and the creation date of the key does not change, only the expiry
// of the subkey does. rpm refuses packages signed by the expired subkey, so the renewed key
// must reach rpm before such a package is installed.
#include <set>

#include <boost/test/unit_test.hpp>

#include <tests/lib/TestSetup.h>
#include <zypp/KeyRing.h>
#include <zypp/target/rpm/RpmDb.h>
#include <zypp/target/private/keyrefresh_p.h>

#include "KeyRingTestReceiver.h"
#include "KeyRenewalFixture.h"

using namespace zypp;
using namespace keyrenewal;

static TestSetup test( TestSetup::initLater );
struct TestInit {
  TestInit() {
    test = TestSetup( Arch_x86_64, TSO_REPO_DEFAULT_GPG );
    getZYpp()->keyRing()->allowPreload( false );
  }
  ~TestInit() { test.reset(); }
};
BOOST_GLOBAL_FIXTURE( TestInit );

static std::set<Edition> rpmKeys()
{ return test.target().rpmDb().pubkeyEditions(); }

static Date subkeyExpires( const PublicKeyData & key_r )
{
  BOOST_REQUIRE( key_r.hasSubkeys() );
  return (*key_r.subkeys().begin()).expires();
}

BOOST_AUTO_TEST_CASE(hook_renews_expired_subkey)
{
  test.target();	// initializes the target; connects the rpm key signal receiver
  KeyRingTestReceiver keyring_callbacks;

  PublicKey v0( dataDir()/"sub-v0.asc" ), v1( dataDir()/"sub-v1.asc" );
  BOOST_REQUIRE_EQUAL( v0.fingerprint(), v1.fingerprint() );
  BOOST_REQUIRE_EQUAL( v0.expires(), Date() );			// the primary key never expires
  BOOST_REQUIRE_EQUAL( v0.created(), v1.created() );		// the creation date does not tell...
  BOOST_REQUIRE( subkeyExpires( v0.keyData() ) < subkeyExpires( v1.keyData() ) );
  const std::string subkeyId { (*v0.subkeys().begin()).id() };
  KeyRing_Ptr ring = getZYpp()->keyRing();

  // The user trusts v0, whose subkey has expired. Silent import: only zypp's trusted ring, rpm stays empty.
  ring->multiKeyImport( dataDir()/"sub-v0.asc", true );
  BOOST_REQUIRE( rpmKeys().empty() );
  BOOST_REQUIRE( target::keyNeedsRefresh( ring->trustedPublicKeyData( subkeyId ), subkeyId ) );

  // The repo still offers v0 when it is refreshed; nothing to renew yet.
  filesystem::TmpDir repoDir;
  copyRepo( repoDir.path(), dataDir()/"sub-v0.asc", "repo-sub" );
  RepoInfo repo;
  repo.setAlias( "renewal" );
  repo.setBaseUrl( repoDir.path().asDirUrl() );
  test.loadRepo( repo );
  BOOST_REQUIRE( rpmKeys().empty() );

  // The vendor renews the subkey without touching the metadata. No refresh happens.
  BOOST_REQUIRE_EQUAL( filesystem::copy( dataDir()/"sub-v1.asc", repoDir.path()/"repodata/repomd.xml.key" ), 0 );

  PoolItem pi;
  for ( const auto & p : test.pool().byName( "pkg-test42" ) )
    if ( ! p.isSystem() ) pi = p;
  BOOST_REQUIRE( pi );

  // Install time: the package (signed by the subkey) is provided, the hook looks at its signing key.
  target::KeyRefreshTracker tracker;
  target::refreshExpiringKey( pi, ManagedFile( repoDir.path()/"x86_64/pkg.rpm" ), tracker );

  // rpm holds exactly this key, with the renewed subkey. Do not compare the release (editionOf):
  // rpm derives it from the newest signature of any kind, including the subkey binding signature,
  // zypp from the user id self-signature only, and they differ for a subkey-only renewal.
  BOOST_REQUIRE_EQUAL( rpmKeys().size(), 1 );
  const std::list<PublicKey> rpmPubkeys { test.target().rpmDb().pubkeys() };
  BOOST_REQUIRE_EQUAL( rpmPubkeys.size(), 1 );
  BOOST_CHECK_EQUAL( rpmPubkeys.front().fingerprint(), v1.fingerprint() );
  BOOST_CHECK_EQUAL( subkeyExpires( rpmPubkeys.front().keyData() ), subkeyExpires( v1.keyData() ) );

  BOOST_CHECK_EQUAL( subkeyExpires( ring->trustedPublicKeyData( subkeyId ) ), subkeyExpires( v1.keyData() ) );
  BOOST_CHECK( ! target::keyNeedsRefresh( ring->trustedPublicKeyData( subkeyId ), subkeyId ) );
  BOOST_CHECK( ! tracker.firstTime( v0.id(), "renewal" ) );	// the attempt was recorded

  BOOST_CHECK( ! keyring_callbacks.askedAcceptKey() );
  BOOST_CHECK( ! keyring_callbacks.askedAcceptUnknownKey() );
  BOOST_CHECK( ! keyring_callbacks.askedAcceptVerFailed() );
  BOOST_CHECK( ! keyring_callbacks.askedAcceptUnsignedFile() );
}
