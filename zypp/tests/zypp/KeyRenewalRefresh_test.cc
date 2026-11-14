// bsc#1284494: Refreshing a repo that offers a renewed version of a trusted key must pass the
// renewed key on to rpm, even though the trusted key is not expired.
#include <set>

#include <boost/test/unit_test.hpp>

#include <tests/lib/TestSetup.h>
#include <zypp/KeyRing.h>
#include <zypp/target/rpm/RpmDb.h>

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

BOOST_AUTO_TEST_CASE(refresh_passes_renewed_key_to_rpm)
{
  test.target();	// initializes the target; connects the rpm key signal receiver
  KeyRingTestReceiver keyring_callbacks;

  PublicKey v2( dataDir()/"key-v2.asc" );
  KeyRing_Ptr ring = getZYpp()->keyRing();

  // The user trusts v1 (silent import: only zypp's trusted ring, rpm stays empty).
  ring->multiKeyImport( dataDir()/"key-v1.asc", true );
  BOOST_REQUIRE( rpmKeys().empty() );

  // The repo offers the renewed key v2.
  filesystem::TmpDir repoDir;
  copyRepo( repoDir.path(), dataDir()/"key-v2.asc" );
  RepoInfo repo;
  repo.setAlias( "renewal" );
  repo.setBaseUrl( repoDir.path().asDirUrl() );

  test.loadRepo( repo );	// refresh + build cache: fetches the repo keys

  BOOST_CHECK( rpmKeys() == std::set<Edition>( { editionOf( v2 ) } ) );
  BOOST_CHECK_EQUAL( ring->trustedPublicKeyData( v2.id() ).created(), v2.created() );

  BOOST_CHECK( ! keyring_callbacks.askedAcceptKey() );
  BOOST_CHECK( ! keyring_callbacks.askedAcceptUnknownKey() );
  BOOST_CHECK( ! keyring_callbacks.askedAcceptVerFailed() );
  BOOST_CHECK( ! keyring_callbacks.askedAcceptUnsignedFile() );
}
