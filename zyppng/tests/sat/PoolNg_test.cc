#include <boost/test/unit_test.hpp>

#include <zypp/ng/sat/pool.h>
#include <zypp/ng/sat/repository.h>
#include <zypp/ng/sat/solvable.h>
#include <zypp/ng/sat/stringpool.h>
#include <zypp/ng/sat/lookupattr.h>
#include <zypp/ng/sat/solvattr.h>
#include <zypp/ng/sat/capability.h>
#include <zypp/ng/sat/preparedpool.h>
#include <zypp/ng/sat/components/poolcomponents.h>

#include <solv/repo.h>

using namespace zyppng;

struct PoolFixture {
  PoolFixture() {
    pool.clear();
  }
  ~PoolFixture() {
    pool.clear();
  }
  sat::Pool pool;
};

BOOST_FIXTURE_TEST_SUITE(PoolTest, PoolFixture)

BOOST_AUTO_TEST_CASE(init)
{
  BOOST_CHECK( pool.get() != nullptr );
  // System repo should be present but empty
  BOOST_CHECK( pool.systemRepo().solvablesSize () == 0 );
}

BOOST_AUTO_TEST_CASE(createRepo)
{
  sat::Repository repo = pool.reposInsert("test-repo");
  BOOST_REQUIRE( repo );

  BOOST_CHECK_EQUAL( repo.alias(), "test-repo" );

  // Verify handle can extract pool
  BOOST_CHECK( repo.pool().get() == pool.get() );
}

BOOST_AUTO_TEST_CASE(prepare)
{
  // Should not crash on empty pool
  BOOST_CHECK_NO_THROW( pool.prepare() );

  SerialNumberWatcher watch( pool.serial() );
  pool.prepare();
  // Serial should not change if nothing was modified
  BOOST_REQUIRE( ! watch.isDirty( pool.serial() ) );
}

BOOST_AUTO_TEST_CASE(solvableHandle)
{
  sat::Repository repo = pool.reposInsert("test-repo");
  sat::detail::SolvableIdType sid = repo.addSolvables( 1 );

  sat::Solvable slv( sid );
  BOOST_CHECK( slv.pool().get() == pool.get() );
  BOOST_CHECK( slv.repository() == repo.id() );
}

BOOST_AUTO_TEST_CASE(lookupAttr)
{
  sat::Repository repo = pool.reposInsert("test-repo");
  sat::detail::SolvableIdType sid = repo.addSolvables( 1 );
  sat::Solvable slv( sid );

  // Set a name manually via libsolv to test lookup
  slv.get()->name = IdString("test-package").id();

  // Test whole-pool lookup (requires explicit pool)
  sat::LookupAttr q1( pool, sat::SolvAttr::name );
  bool found = false;
  for ( auto it = q1.begin(); it != q1.end(); ++it )
  {
    if ( it.asString() == "test-package" )
      found = true;
  }
  BOOST_CHECK( found );

  // Test handle-based lookup (extracts pool from solvable)
  sat::LookupAttr q2( sat::SolvAttr::name, slv );
  BOOST_CHECK_EQUAL( q2.size(), 1 );
  BOOST_CHECK_EQUAL( q2.begin().asString(), "test-package" );
}

// ---------------------------------------------------------------------------
// PreparedPool / whatprovides index tests
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(preparedPoolBuildsIndex)
{
  // Before prepare(): index not yet built.
  BOOST_CHECK( pool.get()->whatprovides == nullptr );

  auto pp = pool.prepare();

  // After prepare(): index must be present.
  BOOST_CHECK( pool.get()->whatprovides != nullptr );
  // PreparedPool must reference the same underlying pool.
  BOOST_CHECK( pp.get() == pool.get() );
}

BOOST_AUTO_TEST_CASE(preparedPoolIndexRebuiltAfterDirty)
{
  pool.prepare();
  BOOST_REQUIRE( pool.get()->whatprovides != nullptr );

  // Invalidate the pool — setDirty() calls pool_freewhatprovides() synchronously.
  pool.setDirty( sat::PoolInvalidation::Data, {"test invalidation"} );
  BOOST_CHECK( pool.get()->whatprovides == nullptr );

  // Re-prepare rebuilds it.
  auto pp2 = pool.prepare();
  BOOST_CHECK( pool.get()->whatprovides != nullptr );
  BOOST_CHECK( pp2.get() == pool.get() );
}

BOOST_AUTO_TEST_CASE(preparedPoolWhatProvidesFindsCapability)
{
  sat::Repository repo = pool.reposInsert("test-repo");
  sat::detail::SolvableIdType sid = repo.addSolvables( 1 );
  sat::Solvable slv( sid );

  // Build the capability we want to provide and then query.
  sat::Capability cap( "test-cap" );
  BOOST_REQUIRE( cap.id() != sat::detail::noId );

  // Append the capability to the solvable's provides list.
  // repo_addid_dep is the only way to append to a solvable's dep list —
  // no NG wrapper exists yet, so this raw call is acceptable.
  // Note: libsolv uses dep_provides (LIBSOLV_SOLVABLE_PREPEND_DEP layout).
  slv.get()->dep_provides = ::repo_addid_dep(
      slv.get()->repo, slv.get()->dep_provides, cap.id(), 0 );

  auto pp = pool.prepare();
  BOOST_REQUIRE( pool.get()->whatprovides != nullptr );

  // Walk the whatprovides data for our capability.
  unsigned offset = pp.whatProvidesCapabilityId( cap.id() );
  bool found = false;
  for ( sat::detail::IdType id = pp.whatProvidesData( offset );
        id != 0;
        id = pp.whatProvidesData( ++offset ) )
  {
    if ( static_cast<sat::detail::SolvableIdType>(id) == sid )
      found = true;
  }
  BOOST_CHECK_MESSAGE( found, "solvable not found in whatprovides for test-cap" );
}

// ---------------------------------------------------------------------------
// Component ordering: IPoolComponent runs before index, IPreparedPoolComponent after
// ---------------------------------------------------------------------------

// Probe that verifies the whatprovides index is NOT yet built when it runs.
struct PreIndexProbe : public sat::IPoolComponent {
  void prepare( sat::Pool & p ) override {
    // IPoolComponent runs before pool_createwhatprovides — index must be absent.
    sawNullIndex = ( p.get()->whatprovides == nullptr );
  }
  bool sawNullIndex = false;
};

// Probe that verifies the whatprovides index IS built when it runs.
struct PostIndexProbe : public sat::IPreparedPoolComponent {
  void prepare( sat::PreparedPool & pp ) override {
    // IPreparedPoolComponent runs after pool_createwhatprovides — index must be present.
    sawNonNullIndex = ( pp.get()->whatprovides != nullptr );
  }
  bool sawNonNullIndex = false;
};

BOOST_AUTO_TEST_CASE(iPoolComponentRunsBeforeIndex)
{
  auto & probe = pool.component<PreIndexProbe>();
  pool.prepare();
  BOOST_CHECK_MESSAGE( probe.sawNullIndex,
      "IPoolComponent::prepare() ran after the whatprovides index was built" );
}

BOOST_AUTO_TEST_CASE(iPreparedPoolComponentRunsAfterIndex)
{
  auto & probe = pool.component<PostIndexProbe>();
  pool.prepare();
  BOOST_CHECK_MESSAGE( probe.sawNonNullIndex,
      "IPreparedPoolComponent::prepare() ran before the whatprovides index was built" );
}

// ---------------------------------------------------------------------------
// TC-5: clear() drops the whatprovides index.
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(clearDropsWhatProvidesIndex)
{
  pool.prepare();
  BOOST_REQUIRE( pool.get()->whatprovides != nullptr );

  pool.clear();

  BOOST_CHECK( pool.get()->whatprovides == nullptr );
}

// ---------------------------------------------------------------------------
// TC-6: Constructing a Solvable with noSolvableId is safe.
//        The null handle evaluates false and its raw pointer is null.
//        Calling pool() on a noSolvable is undefined — do not test that here.
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(noSolvableHandleIsSafe)
{
  sat::Solvable noSlv( sat::detail::noSolvableId );

  // Must not crash or fire an assertion.
  BOOST_CHECK( !noSlv );
  BOOST_CHECK( noSlv.get() == nullptr );
  BOOST_CHECK_EQUAL( noSlv.id(), sat::detail::noSolvableId );
}

// ---------------------------------------------------------------------------
// TC-7: Iterating over solvables on an empty pool does not crash.
//        Exercises the nextInPool() guard against noSolvableId.
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(solvableIteratorOnEmptyPoolDoesNotCrash)
{
  // pool is already cleared by the fixture ctor.
  BOOST_CHECK_NO_THROW({
    int count = 0;
    for ( const auto & slv : pool.solvables() )
    {
      (void)slv;
      ++count;
    }
    // An empty pool has no valid solvables.
    BOOST_CHECK_EQUAL( count, 0 );
  });
}

BOOST_AUTO_TEST_SUITE_END()
