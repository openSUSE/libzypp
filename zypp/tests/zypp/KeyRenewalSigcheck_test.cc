// bsc#1289557: If zypp checks the signature of a package itself (unsigned repos, pkg_gpgcheck), rpm
// refuses a package signed with a renewed key as long as its copy of the key is expired. The signature
// check renews the key from the repo and checks again. This tests what the check relies on: the key
// is renewed, and the caller is told whether it was.
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

BOOST_AUTO_TEST_CASE(sigcheck_renews_expired_key)
{
  test.target();	// initializes the target; connects the rpm key signal receiver
  KeyRingTestReceiver keyring_callbacks;

  PublicKey v0( dataDir()/"key-v0.asc" ), v2( dataDir()/"key-v2.asc" );
  KeyRing_Ptr ring = getZYpp()->keyRing();

  // The user trusts v0, which has expired. Silent import: only zypp's trusted ring, rpm stays empty.
  ring->multiKeyImport( dataDir()/"key-v0.asc", true );
  BOOST_REQUIRE( rpmKeys().empty() );

  // The repo offers v0 only, nothing to renew.
  filesystem::TmpDir repoDir;
  copyRepo( repoDir.path(), dataDir()/"key-v0.asc" );
  RepoInfo repo;
  repo.setAlias( "renewal" );
  repo.setBaseUrl( repoDir.path().asDirUrl() );
  test.loadRepo( repo );
  const Pathname pkg { repoDir.path()/"x86_64/pkg.rpm" };	// signed with the renewed key

  // The package signature check uses the repo info of the package, as probed by the repo manager.
  PoolItem pi;
  for ( const auto & p : test.pool().byName( "pkg-test42" ) )
    if ( ! p.isSystem() ) pi = p;
  BOOST_REQUIRE( pi );
  const RepoInfo pkgRepo { pi.repoInfo() };

  // The key is expired, so the repo keys are fetched and the caller checks again. Nothing was renewed though.
  BOOST_CHECK( target::renewExpiringSigningKey( pkgRepo, pkg ) );
  BOOST_CHECK( rpmKeys().empty() );

  // The vendor renews the key, the repo offers v2.
  BOOST_REQUIRE_EQUAL( filesystem::copy( dataDir()/"key-v2.asc", repoDir.path()/"repodata/repomd.xml.key" ), 0 );

  BOOST_CHECK( target::renewExpiringSigningKey( pkgRepo, pkg ) );
  BOOST_CHECK( rpmKeys() == std::set<Edition>( { editionOf( v2 ) } ) );
  BOOST_CHECK_EQUAL( ring->trustedPublicKeyData( v2.id() ).created(), v2.created() );

  // Nothing left to renew.
  BOOST_CHECK( ! target::renewExpiringSigningKey( pkgRepo, pkg ) );

  // Not a signed package: nothing to do.
  BOOST_CHECK( ! target::renewExpiringSigningKey( pkgRepo, dataDir()/"key-v0.asc" ) );
  BOOST_CHECK( ! target::renewExpiringSigningKey( pkgRepo, Pathname() ) );

  BOOST_CHECK( ! keyring_callbacks.askedAcceptKey() );
  BOOST_CHECK( ! keyring_callbacks.askedAcceptUnknownKey() );
  BOOST_CHECK( ! keyring_callbacks.askedAcceptVerFailed() );
  BOOST_CHECK( ! keyring_callbacks.askedAcceptUnsignedFile() );
}
