#ifndef TESTTOOLS_H
#define TESTTOOLS_H

#include <string>
#include <zypp-core/Pathname.h>

namespace TestTools {
  // read all contents of a file into a string)
  std::string readFile ( const zypp::Pathname &file );

  // write all data as content into a new file
  bool writeFile ( const zypp::Pathname &file, const std::string &data );

  bool checkLocalPort( int portno_r );
}


#define ZYPP_REQUIRE_THROW( statement, exception ) \
  try { BOOST_REQUIRE_THROW(  statement, exception  ); } catch( ... ) { BOOST_FAIL( #statement" throws unexpected exception"); }

#ifdef __cpp_impl_coroutine

#define ZYPP_CORO_FIXTURE( test_name ) \
    struct test_name##_Fixture { \
         zyppng::Task<void> coro_body(); \
    };


#define ZYPP_CORO_BOILERPLATE \
     auto ev = zyppng::EventLoop::create ();  \
     auto res = coro_body (); \
     res.start(); \
     if ( !res.isReady () ) { \
       res.registerNotifyCallback( [&](){ ev->quit(); } ); \
       ev->run(); \
     } \
     /* trigger exception in case there was one */ \
     res.get();

#define ZYPP_CORO_TEST_CASE(test_name) \
    ZYPP_CORO_FIXTURE( test_name ) \
    BOOST_FIXTURE_TEST_CASE(test_name, test_name##_Fixture) { \
      ZYPP_CORO_BOILERPLATE  \
    }\
    zyppng::Task<void> test_name##_Fixture::coro_body()

#endif

#endif // TESTTOOLS_H
