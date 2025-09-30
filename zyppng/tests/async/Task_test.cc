#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include <zypp-core/ng/async/awaitable.h>
#include <zypp-core/ng/base/eventloop.h>
#include <zypp-core/ng/base/timer.h>

//#include <iostream>


template<typename Res>
zyppng::Task<Res> returnAsync( Res &&r, uint64_t timeout = 0 ) {
  // shared_ptr because std::function needs a callable to be copy constructible
  auto p = std::make_shared<typename zyppng::Task<Res>::Promise>();
  auto res = p->get_return_object();

  auto timer = zyppng::Timer::create ();
  auto callback = [ t = timer, p = std::move(p), r = std::forward<Res>(r) ]( auto &) mutable {
    p->return_value( std::move(r) );
  };

  timer->connectFunc( &zyppng::Timer::sigExpired, std::move(callback) );
  timer->setSingleShot(true);
  timer->start(timeout);
  return res;
}


template<typename T>
zyppng::Task<T> coro_func ( T &&val ) {
  auto r = co_await returnAsync( std::forward<T>(val), 0 );
  co_return r;
}

BOOST_AUTO_TEST_CASE( coro_task )
{
  zyppng::EventLoopRef l = zyppng::EventLoop::create();

  auto async_res = coro_func( 10 );

  async_res.registerNotifyCallback ([&](){
    l->quit();
  });


  if ( !async_res.isReady() )
    l->run();

  BOOST_REQUIRE( async_res.isReady () );
  BOOST_REQUIRE_EQUAL( async_res.get (), 10 );
}


BOOST_AUTO_TEST_CASE ( task_destroy_notify )
{
  zyppng::EventLoopRef l = zyppng::EventLoop::create();

  zyppng::Task<int>::Promise p;
  std::optional<zyppng::Task<int>> retObj = p.get_return_object();

  bool destroyTriggered = false;
  const auto &destroyCb = [&](){
    destroyTriggered = true;
  };

  retObj->registerDestroyCallback( destroyCb );
  retObj.reset ();
  BOOST_REQUIRE(destroyTriggered);
}

zyppng::Task<void> coro_throw (  ) {
  throw zypp::Exception("Test");
  co_return;
}

zyppng::Task<void> coro_await_throw ( ) {
  co_await coro_throw();
  co_return;
}

// check if a exception is caught and distributed upwards
BOOST_AUTO_TEST_CASE( coro_exception )
{
  zyppng::EventLoopRef l = zyppng::EventLoop::create();

  auto async_res = coro_await_throw( );

  async_res.registerNotifyCallback ([&](){
    l->quit();
  });

  if ( !async_res.isReady() )
    l->run();

  BOOST_REQUIRE( async_res.isReady () );
  BOOST_REQUIRE_THROW( async_res.get (), zypp::Exception );
}

BOOST_AUTO_TEST_CASE( coro_pipeline )
{

  zyppng::EventLoopRef l = zyppng::EventLoop::create();

  const auto &produce = []() -> int { return 10; };
  const auto &add10   = []( int in ) -> int { return in + 10; };

  using namespace zyppng::operators;

  auto async_res = produce() | []( int in ) -> zyppng::Task<int> { return returnAsync( std::move(in), 10 ); } | add10;

  if ( !async_res.isReady() ) {
    async_res.registerNotifyCallback ([&](){
      l->quit();
    });

    l->run();
  }

  BOOST_REQUIRE( async_res.isReady () );
  BOOST_REQUIRE_EQUAL( async_res.get(), 20  );
}

