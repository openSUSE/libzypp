#include <boost/test/unit_test.hpp>
#include <zypp-core/ng/pipelines/AsyncResult>
#include <zypp-core/ng/pipelines/Transform>
#include <zypp-core/ng/base/EventLoop>
#include <zypp-core/ng/base/Timer>

using namespace zyppng;

template<typename T>
class DelayedValue : public AsyncOp<T>
{
public:
  DelayedValue( T &&value ) {
    _timer = zyppng::Timer::create();
    _timer->setSingleShot ( true );
    _timer->sigExpired().connect([this, value = std::move(value) ]( zyppng::Timer & ) mutable {
      this->setReady( std::move(value) );
    });
    _timer->start ( 10 );
  }
private:
  zyppng::TimerRef _timer;
};


template<typename T, auto outFunc>
class DelayedOp : public AsyncOp<std::invoke_result_t<decltype(outFunc),T&&>>
{
public:
  void operator()( T &&in )
  {
    _timer = zyppng::Timer::create();
    _timer->setSingleShot ( true );
    _timer->sigExpired().connect([this, forwardArg = std::move(in)]( zyppng::Timer & ) mutable {
      this->setReady( std::invoke( outFunc, std::move(forwardArg)));
    });
    _timer->start ( 0 );
  }
private:
  zyppng::TimerRef _timer;
};

// we can not use primitive types with operator|
struct Int {
  Int( int val ) : value(val) {}

  bool operator== ( const Int &other ) const { return value == other.value; }

  int value;
};

Int addFive ( Int &&in )
{
  in.value += 5;
  return in;
}

Int toSignedInt ( std::string &&in ) {
  return Int{ std::stoi( in ) };
}

std::string toString ( Int &&in )
{
  return std::to_string(in.value);
}

// simple sync pipeline, returning the value directly
BOOST_AUTO_TEST_CASE(simplesyncpipe)
{
  using namespace zyppng::operators;
  BOOST_CHECK_EQUAL( std::string("10"), ( std::string("5") | &toSignedInt | &addFive | &toString ) );
}

BOOST_AUTO_TEST_CASE( syncToAsyncPipeline )
{
  using namespace zyppng::operators;

  zyppng::EventLoopRef ev = zyppng::EventLoop::create();
  auto op = std::string("5")
            | std::make_shared<DelayedOp<std::string, toSignedInt>>()
            | std::make_shared<DelayedOp<Int, addFive>>()
            | std::make_shared<DelayedOp<Int, toString>>()
            | [&]( auto && res ){
                BOOST_CHECK_EQUAL ( std::string("10") , res );
                ev->quit ();
                return res;
              };
  BOOST_CHECK ( !op->isReady () );
  ev->run();
}

BOOST_AUTO_TEST_CASE( asyncToAsyncPipeline )
{
  using namespace zyppng::operators;

  zyppng::EventLoopRef ev = zyppng::EventLoop::create();

  auto op = std::make_shared<DelayedValue<std::string>>("5")
            | std::make_shared<DelayedOp<std::string, toSignedInt>>()
            | std::make_shared<DelayedOp<Int, addFive>>()
            | std::make_shared<DelayedOp<Int, toString>>()
            | [&]( auto && res ){
                BOOST_CHECK_EQUAL ( std::string("10") , res );
                ev->quit ();
                return res;
              };
  BOOST_CHECK ( !op->isReady () );
  ev->run();
}

BOOST_AUTO_TEST_CASE( syncToMixedPipeline )
{
  using namespace zyppng::operators;

  zyppng::EventLoopRef ev = zyppng::EventLoop::create();
  auto op = std::string("5")
            | std::make_shared<DelayedOp<std::string, toSignedInt>>()
            | &addFive
            | &toString
            | [&]( auto && res ){
                BOOST_CHECK_EQUAL ( std::string("10") , res );
                ev->quit ();
                return res;
              };
  BOOST_CHECK ( !op->isReady () );
  ev->run();
}

BOOST_AUTO_TEST_CASE( asyncToMixedPipeline )
{
  using namespace zyppng::operators;

  zyppng::EventLoopRef ev = zyppng::EventLoop::create();

  auto op = zyppng::AsyncOpRef<std::string>(std::make_shared<DelayedValue<std::string>>("5"))
            | &toSignedInt
            | std::make_shared<DelayedOp<Int, addFive>>()
            | &toString
            | [&]( auto && res ){
                BOOST_CHECK_EQUAL ( std::string("10") , res );
                ev->quit ();
                return res;
              };
  BOOST_CHECK ( !op->isReady () );
  ev->run();
}

BOOST_AUTO_TEST_CASE( asyncToMixedPipelineWithIndirectAsyncCB )
{
  using namespace zyppng::operators;

  zyppng::EventLoopRef ev = zyppng::EventLoop::create();

  auto op = zyppng::AsyncOpRef<std::string>(std::make_shared<DelayedValue<std::string>>("5"))
            | &toSignedInt
            | []( auto &&in ){ in.value += 5; return std::make_shared<DelayedValue<Int>>(std::move(in)); }
            | &toString
            | [&]( auto && res ){
                BOOST_CHECK_EQUAL ( std::string("10") , res );
                ev->quit ();
                return res;
              };
  BOOST_CHECK ( !op->isReady () );
  ev->run();
}

BOOST_AUTO_TEST_CASE( asyncToMixedPipelineWithIndirectAsyncCBInStdFunction )
{
  using namespace zyppng::operators;

  zyppng::EventLoopRef ev = zyppng::EventLoop::create();

  const auto &makePipeline = [&](){

    const std::function< AsyncOpRef<Int>( Int && ) > &addFiveAsync = []( auto &&in ){ in.value += 5; return std::make_shared<DelayedValue<Int>>(std::move(in)); };

    return zyppng::AsyncOpRef<std::string>(std::make_shared<DelayedValue<std::string>>("5"))
           | &toSignedInt
           | addFiveAsync
           | &toString
           | [&]( auto && res ){
               BOOST_CHECK_EQUAL ( std::string("10") , res );
               ev->quit ();
               return res;
             };
  };

  auto op = makePipeline();
  BOOST_CHECK ( !op->isReady () );
  ev->run();
}

BOOST_AUTO_TEST_CASE( syncTransform )
{
  using namespace zyppng::operators;
  std::vector<Int> input{ 0, 10, 20 };
  auto res = std::vector<Int>(input) | transform( &addFive );
  static_assert( std::is_same_v<std::vector<Int>, decltype(res)> );
  BOOST_CHECK_EQUAL ( res.size(), 3 );
  BOOST_CHECK_EQUAL ( res[0].value, 5 );
  BOOST_CHECK_EQUAL ( res[1].value, 15 );
  BOOST_CHECK_EQUAL ( res[2].value, 25 );
}

BOOST_AUTO_TEST_CASE( asyncTransform )
{
  using namespace zyppng::operators;

  zyppng::EventLoopRef ev = zyppng::EventLoop::create();

  std::vector<Int> input{ 0, 10, 20 };
  auto res = std::vector<Int>(input) | transform( []( auto &&in ){ in.value += 5; return std::make_shared<DelayedValue<Int>>(std::move(in)); } );
  static_assert( std::is_same_v<AsyncOpRef<std::vector<Int>>, decltype(res)> );
  BOOST_CHECK ( !res->isReady () );

  res->onReady ([&]( std::vector<Int> &&res ){
    BOOST_CHECK_EQUAL ( res.size(), 3 );
    BOOST_CHECK_EQUAL ( res[0].value, 5 );
    BOOST_CHECK_EQUAL ( res[1].value, 15 );
    BOOST_CHECK_EQUAL ( res[2].value, 25 );
    ev->quit();
  });
}

//------------------- Code following here is a compile time test, if it compiles it works as intended ------------------------------------

template <typename In, typename Out>
class TestAsyncOp : public AsyncOp<Out>
{
public:
  void operator()( In && ){}
};

int f1Global ( int &&test ){
  return test;
};

template<typename T>
auto f2Global ( T &&test ) {
  return AsyncOpRef<std::remove_reference_t<decltype(test)>>();
};

// using fake lambdas here because it seems to be impossible to use real ones in decltype()
struct FakeLambda
{
  int operator()( int &&arg ) {
    return arg;
  }
};

struct FakeLambda2
{
  template <typename T>
  auto operator()( T &&arg ) {
    return arg;
  }
};

struct FakeLambdaAsync
{
  AsyncOpRef<int> operator()( int &&arg ) {
    return AsyncOpRef<int>();
  }
};

struct FakeLambdaAsync2
{
  template <typename T>
  auto operator()( T &&arg ) {
    return f2Global(std::move(arg));
  }
};

void compiletimeTest ()
{
  const auto &makeSyncCallback = []() {
    return [](int &&test) {
      return test;
    };
  };

  const auto &makeASyncCallback = []() {
    return [](int &&test) {
      return AsyncOpRef<int>();
    };
  };

  const auto &f1 = []( int &&test ){
    return test;
  };
  const auto &f2 = []( int &&test ) {
    return AsyncOpRef<int>();
  };

  const auto &f1Auto = []( auto &&test ){
    return test;
  };
  const auto &f2Auto = []( auto &&test ) {
    return AsyncOpRef<std::remove_reference_t<decltype(test)>>();
  };

  static_assert( detail::is_async_op_v<AsyncOp<int>> );
  static_assert( detail::is_async_op_v<AsyncOpRef<int>> );
  static_assert( detail::has_value_type_v<AsyncOp<int>> );
  static_assert( detail::is_future_monad_cb_v<TestAsyncOp<int,int>, int > );
  static_assert( !detail::is_sync_monad_cb_v<TestAsyncOp<int,int>, int > );
  static_assert( detail::is_sync_monad_cb_v<decltype(f1), int > );
  static_assert( !detail::is_future_monad_cb_v<decltype(f1), int > );
  static_assert( detail::is_sync_monad_cb_v<decltype(f1Auto), int > );
  static_assert( !detail::is_future_monad_cb_v<decltype(f1Auto), int > );
  static_assert( !detail::is_future_monad_cb_v<decltype(f1Auto), AsyncOp<int> > );
  static_assert( !detail::callback_returns_async_op<decltype(f1Auto), int>::value );
  static_assert( detail::callback_returns_async_op<decltype(f2Auto), int>::value );
  static_assert( detail::is_sync_monad_cb_with_sync_res_v<decltype(f1), int> );
  static_assert( !detail::is_sync_monad_cb_with_sync_res_v<decltype(f2), int> );
  static_assert( detail::is_sync_monad_cb_with_sync_res_v<decltype(f1Auto), int> );
  static_assert( !detail::is_sync_monad_cb_with_sync_res_v<decltype(f2Auto), int> );
  static_assert( !detail::is_sync_monad_cb_with_sync_res_v<TestAsyncOp<int,int>, int> );
  static_assert( !std::conditional_t< detail::is_async_op_v<AsyncOp<int>>, std::false_type, std::detected_or_t<std::false_type, std::is_invocable, decltype(f1Auto), AsyncOp<int>> >::value );

  using namespace zyppng::operators;

  // case async -> async
  static_assert( std::is_same_v< AsyncOpRef<std::string>, decltype( std::declval<AsyncOpRef<int>>() | std::declval<std::shared_ptr<TestAsyncOp<int,std::string>>>() )> );

  // case sync -> async
  static_assert( std::is_same_v< AsyncOpRef<int>, decltype( std::declval<int>() | std::declval<std::shared_ptr<TestAsyncOp<int,int>>>() )> );

  // case sync -> sync returning a sync value
  static_assert( std::is_same_v< int, decltype( std::declval<int>() | f1  )> );

  // case sync -> generic sync returning a sync value
  static_assert( std::is_same_v< int, decltype( std::declval<int>() | f1Auto  )> );

  // case sync -> sync returning a async value
  static_assert( std::is_same_v< AsyncOpRef<int>, decltype( std::declval<int>() | f2  )> );

  // case sync -> generic sync returning a async value
  static_assert( std::is_same_v< AsyncOpRef<int>, decltype( std::declval<int>() | f2Auto  )> );

  // case sync -> sync returning a sync value , not supported by C++: operator| must have an argument of class or enumerated type
  // static_assert( std::is_same_v< int, decltype( std::declval<int>() | std::declval<int (*)(int&&)>()  )> );

  // case sync -> lambda returning a sync value
  static_assert( std::is_same_v< int, decltype( std::declval<int>() | std::declval<FakeLambda>()  )> );

  // case sync -> generic lambda returning a sync value
  static_assert( std::is_same_v< int, decltype( std::declval<int>() | std::declval<FakeLambda2>()   )> );

  // case sync -> lambda returning a async value
  static_assert( std::is_same_v< AsyncOpRef<int>, decltype( std::declval<int>() | std::declval<FakeLambdaAsync>()  )> );

  // case sync -> generic lambda returning a async value
  static_assert( std::is_same_v< AsyncOpRef<int>, decltype( std::declval<int>() | std::declval<FakeLambdaAsync2>()  )> );

  // case sync -> sync lambda returned by a function
  static_assert( std::is_same_v< int, decltype( std::declval<int>() | makeSyncCallback() ) > );

  // case async -> sync lambda returned by a function
  static_assert( std::is_same_v< AsyncOpRef<int>, decltype( std::declval< AsyncOpRef<int> >() | makeSyncCallback() ) > );

  // case sync -> sync returning a sync value
  static_assert( std::is_same_v< AsyncOpRef<int>, decltype( std::declval<AsyncOpRef<int>>() | &f1Global  )> );

  // case sync -> lambda returning a sync value
  static_assert( std::is_same_v< AsyncOpRef<int>, decltype( std::declval<AsyncOpRef<int>>() | std::declval<FakeLambda>()  )> );

  // case sync -> generic lambda returning a sync value
  static_assert( std::is_same_v< AsyncOpRef<int>, decltype( std::declval<AsyncOpRef<int>>() | std::declval<FakeLambda2>()   )> );

  // case sync -> lambda returning a async value
  static_assert( std::is_same_v< AsyncOpRef<int>, decltype( std::declval<AsyncOpRef<int>>() | std::declval<FakeLambdaAsync>()  )> );

  // case sync -> generic lambda returning a async value
  static_assert( std::is_same_v< AsyncOpRef<int>, decltype( std::declval<AsyncOpRef<int>>() | std::declval<FakeLambdaAsync2>()  )> );

  // case asyng -> sync lambda with async res returned by a function
  static_assert( std::is_same_v< AsyncOpRef<int>, decltype( std::declval< AsyncOpRef<int> >() | makeASyncCallback() ) > );

}
