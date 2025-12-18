#include <boost/test/unit_test.hpp>
#include <zypp-core/ng/async/task.h>
#include <zypp-core/ng/pipelines/operators.h>
#include <zypp-core/ng/pipelines/Transform>
#include <zypp-core/ng/base/EventLoop>
#include <zypp-core/ng/base/Timer>

#include <tests/lib/TestTools.h>

using namespace zyppng;



template<typename T>
class DelayedValue
{
public:
    DelayedValue(const DelayedValue &) = delete;
    DelayedValue &operator=(const DelayedValue &) = delete;


    DelayedValue(DelayedValue &&) = delete;
    DelayedValue &operator=(DelayedValue &&) = delete;

    DelayedValue(T &&value) : _val(std::move(value)) {}

  bool await_ready() const noexcept
  {
    return false;
  }

  void await_suspend(std::coroutine_handle<> cont)
  {
    EventDispatcher::instance ()->invokeAfter([ hdl = std::move(cont) ](){
      hdl.resume();
      return false;
    }, 10 );
  }

  T await_resume() {
    return std::move(_val);
  }


private:
  zyppng::TimerRef _timer;
  T _val;
};

template<typename T>
Task<T> delayValue( T val ) {
  auto r = co_await (DelayedValue( std::move(val) ));
  co_return r;
}

template<typename T, auto outFunc>
class DelayedOp
{
public:

  using Res = std::invoke_result_t<decltype(outFunc), T&&>;

  DelayedOp( T val )  : _forwardArg( std::move(val)) {}
  ~DelayedOp() {}

  DelayedOp(const DelayedOp &) = delete;
  DelayedOp &operator=(const DelayedOp &) = delete;

  DelayedOp(DelayedOp &&) = delete;
  DelayedOp &operator=(DelayedOp &&) = delete;

  bool await_ready() const noexcept { return _val.has_value(); }

  void await_suspend(std::coroutine_handle<> cont)
  {
    _hdl = std::move(cont);

    EventDispatcher::instance ()->invokeAfter([ this ](){
      _val = std::invoke( outFunc, std::move(_forwardArg));
      _hdl.resume ();
      return false;
    }, 0);
  }

  Res await_resume() {
    return std::move(*_val);
  }

private:
  std::coroutine_handle<> _hdl;
  T _forwardArg;
  std::optional<Res> _val;
};

template<typename T, auto outFunc>
Task<typename DelayedOp<T, outFunc>::Res> delayOp( T &&val ) {
  auto r = co_await ( DelayedOp<T, outFunc>( std::forward<T>(val)) );
  co_return r;
}


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
            | delayOp<std::string, toSignedInt>
            | delayOp<Int, addFive>
            | delayOp<Int, toString>
            | [&]( auto res ){
                BOOST_CHECK_EQUAL ( std::string("10") , res );
                ev->quit ();
                return res;
              };

  op.start();
  BOOST_CHECK ( !op.isReady () );
  ev->run();
}

BOOST_AUTO_TEST_CASE( asyncToAsyncPipeline )
{
  using namespace zyppng::operators;

  zyppng::EventLoopRef ev = zyppng::EventLoop::create();

  auto op = delayValue<std::string>("5")
            | delayOp<std::string, toSignedInt>
            | delayOp<Int, addFive>
            | delayOp<Int, toString>
            | [&]( auto && res ){
                BOOST_CHECK_EQUAL ( std::string("10") , res );
                ev->quit ();
                return res;
              };

  op.start();
  BOOST_CHECK ( !op.isReady () );
  ev->run();
}

BOOST_AUTO_TEST_CASE( syncToMixedPipeline )
{
  using namespace zyppng::operators;

  zyppng::EventLoopRef ev = zyppng::EventLoop::create();
  auto op = std::string("5")
            | delayOp<std::string, toSignedInt>
            | &addFive
            | &toString
            | [&]( auto && res ){
                BOOST_CHECK_EQUAL ( std::string("10") , res );
                ev->quit ();
                return res;
              };

  op.start();
  BOOST_CHECK ( !op.isReady () );
  ev->run();
}

BOOST_AUTO_TEST_CASE( asyncToMixedPipeline )
{
  using namespace zyppng::operators;

  zyppng::EventLoopRef ev = zyppng::EventLoop::create();

  auto op = delayValue<std::string>("5")
            | &toSignedInt
            | delayOp<Int, addFive>
            | &toString
            | [&]( auto && res ){
                BOOST_CHECK_EQUAL ( std::string("10") , res );
                ev->quit ();
                return res;
              };
  op.start();
  BOOST_CHECK ( !op.isReady () );
  ev->run();
}

BOOST_AUTO_TEST_CASE( asyncToMixedPipelineWithIndirectAsyncCB )
{
  using namespace zyppng::operators;
  using zyppng::operators::operator|;

  zyppng::EventLoopRef ev = zyppng::EventLoop::create();

  auto op = delayValue<std::string>("5")
            | toSignedInt
            | []( Int in ) {
              in.value += 5; return delayValue(std::move(in));
            }
            | &toString
            | [&]( auto && res ){
                BOOST_CHECK_EQUAL ( std::string("10") , res );
                ev->quit ();
                return res;
              };
  op.start();
  BOOST_CHECK ( !op.isReady () );
  ev->run();
}

BOOST_AUTO_TEST_CASE( asyncToMixedPipelineWithIndirectAsyncCBInStdFunction )
{
  using namespace zyppng::operators;

  zyppng::EventLoopRef ev = zyppng::EventLoop::create();

  const auto &makePipeline = [&](){

    const std::function< Task<Int>( Int && ) > &addFiveAsync = []( auto &&in ) -> Task<Int> { in.value += 5; co_return co_await delayValue(std::move(in)); };

    return delayValue( std::string("5") )
           | &toSignedInt
           | addFiveAsync
           | &toString
           | [&]( auto res ){
               BOOST_CHECK_EQUAL ( std::string("10") , res );
               ev->quit ();
               return res;
             };
  };

  auto op = makePipeline();
  op.start();
  BOOST_CHECK ( !op.isReady () );
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

ZYPP_CORO_TEST_CASE( asyncTransform )
{
  using namespace zyppng::operators;

  zyppng::EventLoopRef ev = zyppng::EventLoop::create();

  std::vector<Int> input{ 0, 10, 20 };

  auto res = co_await( std::vector<Int>(input) | transform( []( Int &&in ){ in.value += 5; return delayValue(std::move(in)); } ) );
  static_assert( std::is_same_v<std::vector<Int>, decltype(res)> );

  BOOST_CHECK_EQUAL ( res.size(), 3 );
  BOOST_CHECK_EQUAL ( res[0].value, 5 );
  BOOST_CHECK_EQUAL ( res[1].value, 15 );
  BOOST_CHECK_EQUAL ( res[2].value, 25 );
}

//------------------- Code following here is a compile time test, if it compiles it works as intended ------------------------------------

template <typename T>
struct TestAwaitable {

    TestAwaitable() = default;
    TestAwaitable(TestAwaitable &&) = default;
    TestAwaitable &operator=(TestAwaitable &&) = default;
    TestAwaitable(const TestAwaitable &) = delete;
    TestAwaitable &operator=(const TestAwaitable &) = delete;

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<>) {}

    T await_resume() {
      if constexpr (!std::is_same_v<void, T>)
        return std::declval<T>();
    }
};

template <typename In, typename Out>
Task<Out> testAsyncOp( In i ) {
  co_return Out();
}

int f1Global ( int &&test ){
  return test;
};

template<typename T>
Task<T> f2Global ( T &&test ) {
  co_return test;
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
  Task<int> operator()( int &&arg ) {
    co_return arg;
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
    return [](int &&test) -> Task<int> {
      co_return test;
    };
  };

  const auto &f1 = []( int &&test ){
    return test;
  };
  const auto &f2 = []( int &&test ) -> Task<int> {
    co_return test;
  };

  const auto &f1Auto = []( auto &&test ){
    return test;
  };
  const auto &f2Auto = []( auto &&test ) -> Task<std::decay_t<decltype(test)>> {
    co_return test;
  };


  static_assert( Awaitable<Task<void>> );
  static_assert( Awaitable<Task<int>> );
  static_assert( !Awaitable<void> );
  static_assert( !Awaitable<int> );
  static_assert( Awaitable<TestAwaitable<int>> );
  static_assert( Awaitable<TestAwaitable<void>> );

  static_assert( AwaiterInterface<TestAwaitable<int>> );
  static_assert( AwaiterInterface<TestAwaitable<void>> );
  static_assert( !AwaiterInterface<Task<int>> );
  static_assert( !AwaiterInterface<Task<void>> );

  static_assert( std::is_same_v< int, awaitable_res_type_t<TestAwaitable<int>>> );
  static_assert( std::is_same_v< void, awaitable_res_type_t<TestAwaitable<void>>> );
  static_assert( std::is_same_v< int, awaitable_res_type_t<Task<int>>> );
  static_assert( std::is_same_v< void, awaitable_res_type_t<Task<void>>> );


  using namespace zyppng::operators;

  // case async -> async
  static_assert( std::is_same_v< Task<std::string>, decltype( std::declval<Task<int>>() | testAsyncOp<int,std::string> ) > );

  // case sync -> async ( need to wrap the callback in std::function because C++ demands one operant to be of class or enumeration type
  static_assert( std::is_same_v< Task<int>, decltype( 10 | std::function(testAsyncOp<int,int>) ) > );

  // case sync -> sync returning a sync value
  static_assert( std::is_same_v< int, decltype( std::declval<int>() | f1  )> );

  // case sync -> generic sync returning a sync value
  static_assert( std::is_same_v< int, decltype( std::declval<int>() | f1Auto  )> );

  // case sync -> sync returning a async value
  static_assert( std::is_same_v< Task<int>, decltype( std::declval<int>() | f2  )> );

  // case sync -> generic sync returning a async value
  static_assert( std::is_same_v< Task<int>, decltype( std::declval<int>() | f2Auto  )> );

  // case sync -> sync returning a sync value , not supported by C++: operator| must have an argument of class or enumerated type
  // static_assert( std::is_same_v< int, decltype( std::declval<int>() | std::declval<int (*)(int&&)>()  )> );

  // case sync -> lambda returning a sync value
  static_assert( std::is_same_v< int, decltype( std::declval<int>() | std::declval<FakeLambda>()  )> );

  // case sync -> generic lambda returning a sync value
  static_assert( std::is_same_v< int, decltype( std::declval<int>() | std::declval<FakeLambda2>()   )> );

  // case sync -> lambda returning a async value
  static_assert( std::is_same_v< Task<int>, decltype( std::declval<int>() | std::declval<FakeLambdaAsync>()  )> );

  // case sync -> generic lambda returning a async value
  static_assert( std::is_same_v< Task<int>, decltype( std::declval<int>() | std::declval<FakeLambdaAsync2>()  )> );

  // case sync -> sync lambda returned by a function
  static_assert( std::is_same_v< int, decltype( std::declval<int>() | makeSyncCallback() ) > );

  // case async -> sync lambda returned by a function
  static_assert( std::is_same_v< Task<int>, decltype( std::declval< Task<int> >() | makeSyncCallback() ) > );

  // case sync -> sync returning a sync value
  static_assert( std::is_same_v< Task<int>, decltype( std::declval<Task<int>>() | &f1Global  )> );

  // case sync -> lambda returning a sync value
  static_assert( std::is_same_v< Task<int>, decltype( std::declval<Task<int>>() | std::declval<FakeLambda>()  )> );

  // case sync -> generic lambda returning a sync value
  static_assert( std::is_same_v< Task<int>, decltype( std::declval<Task<int>>() | std::declval<FakeLambda2>()   )> );

  // case sync -> lambda returning a async value
  static_assert( std::is_same_v< Task<int>, decltype( std::declval<Task<int>>() | std::declval<FakeLambdaAsync>()  )> );

  // case sync -> generic lambda returning a async value
  static_assert( std::is_same_v< Task<int>, decltype( std::declval<Task<int>>() | std::declval<FakeLambdaAsync2>()  )> );

  // case asyng -> sync lambda with async res returned by a function
  static_assert( std::is_same_v< Task<int>, decltype( std::declval< Task<int> >() | makeASyncCallback() ) > );

}
