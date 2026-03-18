#include <boost/test/unit_test.hpp>

#include <zypp/ng/sat/pool.h>
#include <zypp/ng/sat/repository.h>
#include <zypp/ng/sat/preparedpool.h>
#include <zypp/ng/sat/components/poolcomponents.h>

#include <vector>
#include <string>

using namespace zyppng;

// ---------------------------------------------------------------------------
// Test fixture — fresh pool for each test case.
// ---------------------------------------------------------------------------

struct CompSetFixture {
  CompSetFixture() { pool.clear(); }
  ~CompSetFixture() { pool.clear(); }
  sat::Pool pool;
};

BOOST_FIXTURE_TEST_SUITE(PoolComponentSetTest, CompSetFixture)

// ---------------------------------------------------------------------------
// assertComponent — idempotency
// ---------------------------------------------------------------------------

struct IdempotentComp : public sat::IPoolComponent {
  int prepareCount = 0;
  void prepare( sat::Pool & ) override { ++prepareCount; }
};

BOOST_AUTO_TEST_CASE(assertComponentIdempotent)
{
  // First call creates the component.
  auto & a = pool.component<IdempotentComp>();
  // Second call must return the same object, not a new one.
  auto & b = pool.component<IdempotentComp>();
  BOOST_CHECK( &a == &b );

  pool.prepare();
  BOOST_CHECK_EQUAL( a.prepareCount, 1 );

  // Pool serial did not change — prepare() is a no-op on a clean pool.
  // prepareCount must remain 1.
  pool.prepare();
  BOOST_CHECK_EQUAL( a.prepareCount, 1 );
}

// ---------------------------------------------------------------------------
// findComponent — miss and hit
// ---------------------------------------------------------------------------

struct FindCompInit : public sat::IPoolComponent {};
struct FindCompPrep : public sat::IPreparedPoolComponent {};

BOOST_AUTO_TEST_CASE(findComponentMissReturnsNull)
{
  // Nothing registered yet — must return nullptr.
  BOOST_CHECK( pool.findComponent<FindCompInit>() == nullptr );
  BOOST_CHECK( pool.findComponent<FindCompPrep>() == nullptr );
}

BOOST_AUTO_TEST_CASE(findComponentHitReturnsPointer)
{
  auto & reg = pool.component<FindCompInit>();
  const auto * found = pool.findComponent<FindCompInit>();

  BOOST_REQUIRE( found != nullptr );
  BOOST_CHECK( found == &reg );

  // Unrelated type must still miss.
  BOOST_CHECK( pool.findComponent<FindCompPrep>() == nullptr );
}

// ---------------------------------------------------------------------------
// IPoolComponent priority ordering within InitStage::Environment
// ---------------------------------------------------------------------------

// Two distinct IPoolComponent types with different priorities wired to a
// shared log. Registered slow-first to ensure sort overrides insertion order.

struct InitPri1 : public sat::IPoolComponent {
  std::vector<std::string> * log = nullptr;
  int priority() const override { return 1; }
  void prepare( sat::Pool & ) override { if (log) log->push_back("pri1"); }
};

struct InitPri10 : public sat::IPoolComponent {
  std::vector<std::string> * log = nullptr;
  int priority() const override { return 10; }
  void prepare( sat::Pool & ) override { if (log) log->push_back("pri10"); }
};

BOOST_AUTO_TEST_CASE(initPriorityOrder)
{
  std::vector<std::string> log;

  // Register the higher numeric priority (slower) first — sort must reorder.
  auto & slow = pool.component<InitPri10>();
  slow.log = &log;
  auto & fast = pool.component<InitPri1>();
  fast.log = &log;

  pool.prepare();

  BOOST_REQUIRE_EQUAL( log.size(), 2u );
  BOOST_CHECK_EQUAL( log[0], "pri1"  );
  BOOST_CHECK_EQUAL( log[1], "pri10" );
}

// ---------------------------------------------------------------------------
// IPreparedPoolComponent stage ordering: Policy before Metadata
// ---------------------------------------------------------------------------

// Registered Metadata-first to ensure stage ordering overrides insertion order.

struct PrepPolicy : public sat::IPreparedPoolComponent {
  std::vector<std::string> * log = nullptr;
  sat::PreparedStage stage() const override { return sat::PreparedStage::Policy; }
  void prepare( sat::PreparedPool & ) override { if (log) log->push_back("policy"); }
};

struct PrepMetadata : public sat::IPreparedPoolComponent {
  std::vector<std::string> * log = nullptr;
  sat::PreparedStage stage() const override { return sat::PreparedStage::Metadata; }
  void prepare( sat::PreparedPool & ) override { if (log) log->push_back("metadata"); }
};

BOOST_AUTO_TEST_CASE(preparedStageOrder)
{
  std::vector<std::string> log;

  // Register Metadata first — stage ordering must override insertion order.
  auto & meta = pool.component<PrepMetadata>();
  meta.log = &log;
  auto & pol = pool.component<PrepPolicy>();
  pol.log = &log;

  pool.prepare();

  BOOST_REQUIRE_EQUAL( log.size(), 2u );
  BOOST_CHECK_EQUAL( log[0], "policy"   );
  BOOST_CHECK_EQUAL( log[1], "metadata" );
}

// ---------------------------------------------------------------------------
// IPreparedPoolComponent priority ordering within PreparedStage::Policy
// ---------------------------------------------------------------------------

struct PrepPri1 : public sat::IPreparedPoolComponent {
  std::vector<std::string> * log = nullptr;
  int priority() const override { return 1; }
  void prepare( sat::PreparedPool & ) override { if (log) log->push_back("pri1"); }
};

struct PrepPri10 : public sat::IPreparedPoolComponent {
  std::vector<std::string> * log = nullptr;
  int priority() const override { return 10; }
  void prepare( sat::PreparedPool & ) override { if (log) log->push_back("pri10"); }
};

BOOST_AUTO_TEST_CASE(preparedPriorityOrder)
{
  std::vector<std::string> log;

  // Register the higher numeric priority (slower) first — sort must reorder.
  auto & slow = pool.component<PrepPri10>();
  slow.log = &log;
  auto & fast = pool.component<PrepPri1>();
  fast.log = &log;

  pool.prepare();

  BOOST_REQUIRE_EQUAL( log.size(), 2u );
  BOOST_CHECK_EQUAL( log[0], "pri1"  );
  BOOST_CHECK_EQUAL( log[1], "pri10" );
}

// ---------------------------------------------------------------------------
// Sort-staleness: adding a component after the first prepare() re-sets the
// dirty flag so the next prepare() re-sorts the bucket.
// ---------------------------------------------------------------------------

struct StalePri1 : public sat::IPoolComponent {
  std::vector<std::string> * log = nullptr;
  int priority() const override { return 1; }
  void prepare( sat::Pool & ) override { if (log) log->push_back("stale-pri1"); }
};

struct StalePri10 : public sat::IPoolComponent {
  std::vector<std::string> * log = nullptr;
  int priority() const override { return 10; }
  void prepare( sat::Pool & ) override { if (log) log->push_back("stale-pri10"); }
};

BOOST_AUTO_TEST_CASE(sortStalenessResortOnNewComponent)
{
  std::vector<std::string> log;

  // Register the slow component first and run prepare() — the bucket sorts
  // and clears its dirty flag.
  auto & slow = pool.component<StalePri10>();
  slow.log = &log;
  pool.prepare();
  log.clear();

  // Add the fast component. assertComponent() sets the dirty flag for the
  // bucket again, so the next notifyPrepare() will re-sort.
  // Invalidate the pool so prepare() does not short-circuit on a clean serial.
  auto & fast = pool.component<StalePri1>();
  fast.log = &log;
  pool.setDirty( sat::PoolInvalidation::Data, {"staleness test"} );
  pool.prepare();

  // Both components must have run, fast before slow.
  BOOST_REQUIRE_EQUAL( log.size(), 2u );
  BOOST_CHECK_EQUAL( log[0], "stale-pri1"  );
  BOOST_CHECK_EQUAL( log[1], "stale-pri10" );
}

// ---------------------------------------------------------------------------
// checkDirty is fired for both IPoolComponent and IPreparedPoolComponent
// ---------------------------------------------------------------------------

struct CheckDirtyInit : public sat::IPoolComponent {
  int count = 0;
  void checkDirty( sat::Pool & ) override { ++count; }
};

struct CheckDirtyPrep : public sat::IPreparedPoolComponent {
  int count = 0;
  void checkDirty( sat::Pool & ) override { ++count; }
};

BOOST_AUTO_TEST_CASE(checkDirtyFiredForAllComponents)
{
  auto & ic = pool.component<CheckDirtyInit>();
  auto & pc = pool.component<CheckDirtyPrep>();

  pool.prepare();

  // checkDirty() is the probe phase — it fires once per prepare() call
  // regardless of whether the pool is dirty.
  BOOST_CHECK_EQUAL( ic.count, 1 );
  BOOST_CHECK_EQUAL( pc.count, 1 );

  // Second prepare() on a clean pool: checkDirty() still fires (probe phase
  // always runs), but component prepare() does not re-run.
  pool.prepare();
  BOOST_CHECK_EQUAL( ic.count, 2 );
  BOOST_CHECK_EQUAL( pc.count, 2 );
}

// ---------------------------------------------------------------------------
// onInvalidate is fired for both component types
// ---------------------------------------------------------------------------

struct InvalidateInit : public sat::IPoolComponent {
  int count = 0;
  void onInvalidate( sat::Pool &, sat::PoolInvalidation ) override { ++count; }
};

struct InvalidatePrep : public sat::IPreparedPoolComponent {
  int count = 0;
  void onInvalidate( sat::Pool &, sat::PoolInvalidation ) override { ++count; }
};

BOOST_AUTO_TEST_CASE(onInvalidateFiredForAllComponents)
{
  auto & ic = pool.component<InvalidateInit>();
  auto & pc = pool.component<InvalidatePrep>();

  pool.setDirty( sat::PoolInvalidation::Data, {"test"} );
  BOOST_CHECK_EQUAL( ic.count, 1 );
  BOOST_CHECK_EQUAL( pc.count, 1 );

  pool.setDirty( sat::PoolInvalidation::Dependency, {"test"} );
  BOOST_CHECK_EQUAL( ic.count, 2 );
  BOOST_CHECK_EQUAL( pc.count, 2 );
}

// ---------------------------------------------------------------------------
// onRepoAdded / onRepoRemoved are fired for both component types
// ---------------------------------------------------------------------------

struct RepoEventInit : public sat::IPoolComponent {
  int addedCount   = 0;
  int removedCount = 0;
  void onRepoAdded  ( sat::Pool &, sat::detail::RepoIdType ) override { ++addedCount;   }
  void onRepoRemoved( sat::Pool &, sat::detail::RepoIdType ) override { ++removedCount; }
};

struct RepoEventPrep : public sat::IPreparedPoolComponent {
  int addedCount   = 0;
  int removedCount = 0;
  void onRepoAdded  ( sat::Pool &, sat::detail::RepoIdType ) override { ++addedCount;   }
  void onRepoRemoved( sat::Pool &, sat::detail::RepoIdType ) override { ++removedCount; }
};

BOOST_AUTO_TEST_CASE(onRepoAddedFiredForAllComponents)
{
  auto & ic = pool.component<RepoEventInit>();
  auto & pc = pool.component<RepoEventPrep>();

  pool.reposInsert("test-repo-a");

  // reposInsert may also create @System on first access — we only assert
  // that both types received the same count and it is at least 1.
  BOOST_CHECK_GT( ic.addedCount, 0 );
  BOOST_CHECK_EQUAL( ic.addedCount, pc.addedCount );
}

BOOST_AUTO_TEST_CASE(onRepoRemovedFiredForAllComponents)
{
  auto & ic = pool.component<RepoEventInit>();
  auto & pc = pool.component<RepoEventPrep>();

  pool.reposInsert("test-repo-b");
  const int addedBefore = ic.addedCount;

  pool.reposErase("test-repo-b");

  BOOST_CHECK_EQUAL( ic.removedCount, 1 );
  BOOST_CHECK_EQUAL( pc.removedCount, 1 );
  // addedCount must be unchanged after the erase.
  BOOST_CHECK_EQUAL( ic.addedCount, addedBefore );
}

// ---------------------------------------------------------------------------
// TC-1: onReset is fired for both IPoolComponent and IPreparedPoolComponent
//        on pool.clear().
// ---------------------------------------------------------------------------

struct ResetInit : public sat::IPoolComponent {
  int resetCount = 0;
  void onReset( sat::Pool & ) override { ++resetCount; }
};

struct ResetPrep : public sat::IPreparedPoolComponent {
  int resetCount = 0;
  void onReset( sat::Pool & ) override { ++resetCount; }
};

BOOST_AUTO_TEST_CASE(onResetFiredForBothComponentTypes)
{
  auto & ic = pool.component<ResetInit>();
  auto & pc = pool.component<ResetPrep>();

  // clear() is also called by the fixture ctor — reset counters now so we
  // only count the explicit call below.
  ic.resetCount = 0;
  pc.resetCount = 0;

  pool.clear();

  BOOST_CHECK_EQUAL( ic.resetCount, 1 );
  BOOST_CHECK_EQUAL( pc.resetCount, 1 );
}

// ---------------------------------------------------------------------------
// TC-2: onReset and onInvalidate are distinct signals.
//        clear() fires onReset but must NOT fire onInvalidate.
// ---------------------------------------------------------------------------

struct ResetVsInvalidateComp : public sat::IPoolComponent {
  int resetCount      = 0;
  int invalidateCount = 0;
  void onReset     ( sat::Pool & )                    override { ++resetCount; }
  void onInvalidate( sat::Pool &, sat::PoolInvalidation ) override { ++invalidateCount; }
};

BOOST_AUTO_TEST_CASE(onResetIsDistinctFromOnInvalidate)
{
  auto & c = pool.component<ResetVsInvalidateComp>();
  c.resetCount      = 0;
  c.invalidateCount = 0;

  pool.clear();

  BOOST_CHECK_EQUAL( c.resetCount,      1 );
  BOOST_CHECK_EQUAL( c.invalidateCount, 0 );
}

// ---------------------------------------------------------------------------
// TC-3: Components survive pool.clear() — they remain registered and
//        accessible via findComponent<T>() afterwards.
// ---------------------------------------------------------------------------

struct SurvivesClearComp : public sat::IPoolComponent {};

BOOST_AUTO_TEST_CASE(componentSurvivesClear)
{
  auto & before = pool.component<SurvivesClearComp>();

  pool.clear();

  const auto * after = pool.findComponent<SurvivesClearComp>();
  BOOST_REQUIRE( after != nullptr );
  BOOST_CHECK( after == &before );
}

// ---------------------------------------------------------------------------
// TC-4: prepare() re-runs after clear().
//        clear() bumps the serial, so notifyPrepare must fire on the next
//        prepare() call even though no explicit setDirty() was issued.
// ---------------------------------------------------------------------------

struct PrepareAfterClearComp : public sat::IPoolComponent {
  int prepareCount = 0;
  void prepare( sat::Pool & ) override { ++prepareCount; }
};

BOOST_AUTO_TEST_CASE(prepareRunsAfterClear)
{
  auto & c = pool.component<PrepareAfterClearComp>();

  pool.prepare();
  BOOST_REQUIRE_EQUAL( c.prepareCount, 1 );

  pool.clear();

  pool.prepare();
  BOOST_CHECK_EQUAL( c.prepareCount, 2 );
}

BOOST_AUTO_TEST_SUITE_END()
