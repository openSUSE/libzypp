#include <boost/test/unit_test.hpp>
#include <zypp/ng/context.h>
#include <zypp/ng/resource.h>

#include <zypp-core/zyppng/pipelines/expected.h>
#include <zypp-core/zyppng/pipelines/asyncresult.h>
#include <zypp-core/zyppng/base/timer.h>

#include <optional>


BOOST_AUTO_TEST_CASE(simplelock)
{
  auto ctx = zyppng::SyncContext::create();
  auto lExp = ctx->lockResource ( "test" ); // shared lock
  BOOST_REQUIRE( lExp.is_valid () );

  // store in a optional so we can release the lock
  auto l = std::make_optional( std::move(lExp.get()) );
  BOOST_REQUIRE_EQUAL( l->lockIdent(), std::string("test") );

  {
    // now lock again, try exclusive
    auto lExcl = ctx->lockResource ( "test", zyppng::ResourceLockRef::Exclusive );
    BOOST_REQUIRE( !lExcl.is_valid () );
    BOOST_REQUIRE_THROW ( lExcl.unwrap(), zyppng::ResourceAlreadyLockedException );
  }

  // clear the lock
  l.reset();

  {
    // now lock again, try exclusive
    auto lExcl = ctx->lockResource ( "test", zyppng::ResourceLockRef::Exclusive );
    BOOST_REQUIRE_NO_THROW ( lExcl.unwrap() );
    BOOST_REQUIRE_EQUAL( lExcl->lockIdent(), std::string("test") );
  }

}

BOOST_AUTO_TEST_CASE(two_simple_locks)
{
  auto ctx = zyppng::SyncContext::create();
  auto lExp = ctx->lockResource ( "test1", zyppng::ResourceLockRef::Exclusive );
  BOOST_REQUIRE( lExp.is_valid () );
  BOOST_REQUIRE_EQUAL( lExp->lockIdent(), std::string("test1") );

  auto lExp2 = ctx->lockResource ( "test2", zyppng::ResourceLockRef::Exclusive );
  BOOST_REQUIRE( lExp.is_valid () );
  BOOST_REQUIRE_EQUAL( lExp2->lockIdent(), std::string("test2") );
}



BOOST_AUTO_TEST_CASE(asyncLock)
{
  using namespace zyppng::operators;

  auto el = zyppng::EventLoop::create();
  auto ctx = zyppng::AsyncContext::create();

  std::optional<zyppng::ResourceLockRef> l;

  {
    auto expLock = ctx->lockResource ("test", zyppng::ResourceLockRef::Exclusive );
    BOOST_REQUIRE( expLock.is_valid () );
    l.emplace ( std::move(expLock.get()) );
  }

  std::vector<zyppng::ResourceLockRef> sharedLocks;

  const auto &sharedLockCb = [&]( zyppng::expected<zyppng::ResourceLockRef> l ) {

    BOOST_REQUIRE( l.is_valid () );
    if ( !l ) {
      el->quit();
      return zyppng::expected<void>::error( l.error() );
    }

    BOOST_REQUIRE_EQUAL( l->lockIdent (), "test" );

    sharedLocks.emplace_back( std::move(l.get()) );
    if ( sharedLocks.size() == 2 )
      sharedLocks.clear(); // kill all locks, letting the last o ne move on

    return zyppng::expected<void>::success();
  };

  auto asyncLock1 = ctx->lockResourceWait ( "test", 10, zyppng::ResourceLockRef::Shared ) | sharedLockCb;
  auto asyncLock2 = ctx->lockResourceWait ( "test", 10, zyppng::ResourceLockRef::Shared ) | sharedLockCb;
  auto asyncLock3 = ctx->lockResourceWait ( "test", 20, zyppng::ResourceLockRef::Exclusive );

  bool gotLock4 = false;
  auto asyncLock4 = ctx->lockResourceWait ( "test", 30, zyppng::ResourceLockRef::Exclusive )
      | ( [&]( zyppng::expected<zyppng::ResourceLockRef> l ) {
        gotLock4 = true; // should not happen
        return l;
  });

  BOOST_REQUIRE( !asyncLock1->isReady() );
  BOOST_REQUIRE( !asyncLock2->isReady() );
  BOOST_REQUIRE( !asyncLock3->isReady() );
  BOOST_REQUIRE( !asyncLock4->isReady() );

  auto t = zyppng::Timer::create();
  t->connectFunc( &zyppng::Timer::sigExpired, [&]( auto &){ el->quit(); } );
  t->start( 40 );

  // release the existing lock as soon as the ev is running
  zyppng::EventDispatcher::invokeOnIdle([&](){
    l.reset();
    return false;
  });

  el->run();

  BOOST_REQUIRE( asyncLock1->isReady() );
  BOOST_REQUIRE( asyncLock1->get().is_valid() );

  BOOST_REQUIRE( asyncLock2->isReady() );
  BOOST_REQUIRE( asyncLock2->get().is_valid() );

  // third lock should have worked out too after the first 2 were released
  BOOST_REQUIRE( asyncLock3->isReady() );
  BOOST_REQUIRE( asyncLock3->get().is_valid() );

  // fourh lock should fail with timeout
  BOOST_REQUIRE( asyncLock4->isReady() );
  BOOST_REQUIRE( !asyncLock4->get().is_valid() );

  BOOST_REQUIRE_THROW ( asyncLock4->get().unwrap(), zyppng::ResourceLockTimeoutException );

}
