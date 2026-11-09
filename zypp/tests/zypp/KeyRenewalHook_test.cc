// bsc#1289557: Before a package is installed, an expired trusted key must be renewed from the repo,
// even though the repo metadata did not change since the last refresh.
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

BOOST_AUTO_TEST_CASE(hook_renews_expired_key)
{
  test.target();	// initializes the target; connects the rpm key signal receiver
  KeyRingTestReceiver keyring_callbacks;

  PublicKey v0( dataDir()/"key-v0.asc" ), v2( dataDir()/"key-v2.asc" );
  KeyRing_Ptr ring = getZYpp()->keyRing();

  // The user trusts v0, which has expired. Silent import: only zypp's trusted ring, rpm stays empty.
  ring->multiKeyImport( dataDir()/"key-v0.asc", true );
  BOOST_REQUIRE( rpmKeys().empty() );
  BOOST_REQUIRE( target::keyNeedsRefresh( ring->trustedPublicKeyData( v0.id() ), v0.id() ) );

  // The repo still offers v0 when it is refreshed; nothing to renew yet.
  filesystem::TmpDir repoDir;
  copyRepo( repoDir.path(), dataDir()/"key-v0.asc" );
  RepoInfo repo;
  repo.setAlias( "renewal" );
  repo.setBaseUrl( repoDir.path().asDirUrl() );
  test.loadRepo( repo );
  BOOST_REQUIRE( rpmKeys().empty() );

  // The vendor renews the key without touching the metadata. No refresh happens.
  BOOST_REQUIRE_EQUAL( filesystem::copy( dataDir()/"key-v2.asc", repoDir.path()/"repodata/repomd.xml.key" ), 0 );

  PoolItem pi;
  for ( const auto & p : test.pool().byName( "pkg-test42" ) )
    if ( ! p.isSystem() ) pi = p;
  BOOST_REQUIRE( pi );

  // Install time: the package is provided (here: the fixture file), the hook looks at its signing key.
  target::KeyRefreshTracker tracker;
  target::refreshExpiringKey( pi, ManagedFile( repoDir.path()/"x86_64/pkg.rpm" ), tracker );

  BOOST_CHECK( rpmKeys() == std::set<Edition>( { editionOf( v2 ) } ) );
  BOOST_CHECK_EQUAL( ring->trustedPublicKeyData( v2.id() ).created(), v2.created() );
  BOOST_CHECK( ! tracker.firstTime( v0.id(), "renewal" ) );	// the attempt was recorded

  BOOST_CHECK( ! keyring_callbacks.askedAcceptKey() );
  BOOST_CHECK( ! keyring_callbacks.askedAcceptUnknownKey() );
  BOOST_CHECK( ! keyring_callbacks.askedAcceptVerFailed() );
  BOOST_CHECK( ! keyring_callbacks.askedAcceptUnsignedFile() );
}
