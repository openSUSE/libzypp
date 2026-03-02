#include <boost/test/unit_test.hpp>

#include <zypp/ng/sat/pool.h>
#include <zypp/ng/sat/repository.h>
#include <zypp/ng/sat/solvable.h>
#include <zypp/ng/sat/stringpool.h>
#include <zypp/ng/sat/lookupattr.h>
#include <zypp/ng/sat/solvattr.h>

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
  BOOST_CHECK( pool.getPool() != nullptr );
  // System repo should be present but empty
  BOOST_CHECK( pool.systemRepo().solvablesSize () == 0 );
}

BOOST_AUTO_TEST_CASE(createRepo)
{
  sat::detail::CRepo * repo_ptr = pool._createRepo("test-repo");
  BOOST_REQUIRE( repo_ptr != nullptr );

  sat::Repository repo( repo_ptr );
  BOOST_CHECK_EQUAL( repo.alias(), "test-repo" );

  // Verify handle can extract pool
  BOOST_CHECK( repo.myPool().getPool() == pool.getPool() );
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
  sat::detail::CRepo * repo_ptr = pool._createRepo("test-repo");
  sat::detail::SolvableIdType sid = pool._addSolvables( repo_ptr, 1 );

  sat::Solvable slv( sid );
  BOOST_CHECK( slv.myPool().getPool() == pool.getPool() );
  BOOST_CHECK( slv.repository() == repo_ptr );
}

BOOST_AUTO_TEST_CASE(lookupAttr)
{
  sat::detail::CRepo * repo_ptr = pool._createRepo("test-repo");
  sat::detail::SolvableIdType sid = pool._addSolvables( repo_ptr, 1 );
  sat::Solvable slv( sid );

  // Set a name manually via libsolv to test lookup
  slv.get()->name = ::pool_str2id( pool.getPool(), "test-package", 1 );

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

// Dummy component to test the stability loop
struct TestComponent : public sat::IPoolComponent
{
  TestComponent(  ) = default;
  virtual ~TestComponent() = default;

  sat::ComponentStage stage() const override { return sat::ComponentStage::Metadata; }

  void prepare(sat::Pool &pool) override {
    runCount++;
    if ( triggerInvalidation ) {
      triggerInvalidation = false;
      pool.setDirty( sat::PoolInvalidation::Data, {"Testing stability loop"} );
    }
  }

  int runCount = 0;
  bool triggerInvalidation = false;
};


BOOST_AUTO_TEST_CASE(stabilityLoop)
{
  auto & comp = pool.component<TestComponent>();

  // Normal run
  pool.prepare();
  BOOST_CHECK_EQUAL( comp.runCount, 1 );

  // Trigger re-run via invalidation
  comp.triggerInvalidation = true;
  pool.prepare();
  // Should have run twice in the loop
  BOOST_CHECK_EQUAL( comp.runCount, 3 ); // 1 (first run) + 2 (second prepare with invalidation = 3)
}

BOOST_AUTO_TEST_SUITE_END()
