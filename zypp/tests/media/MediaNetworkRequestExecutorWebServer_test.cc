/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | | |                         |
|                         /_____||_| |_| |_| |                         |
|                                                                      |
\----------------------------------------------------------------------/
*
* Cancellable Curl2 (ng) download, exercised through the project's own
* WebServer test rig (a stalling request handler) instead of the raw TCP
* server used by MediaNetworkRequestExecutor_test.
*
* This variant is only built when the media backend tests are enabled
* (WebServer depends on libfcgi), see zypp/tests/media/CMakeLists.txt.
*/
#include <boost/test/unit_test.hpp>

#include <zypp/media/detail/MediaNetworkRequestExecutor.h>
#include <zypp-core/base/UserRequestException>

#include <zypp-curl/ng/network/Request>
#include <zypp-core/ng/base/EventLoop>
#include <zypp-core/ng/base/Timer>
#include <zypp-core/ng/base/private/threaddata_p.h>

#include <zypp-core/Url.h>
#include <zypp/TmpPath.h>
#include <zypp/ZYpp.h>

#include <tests/lib/WebServer.h>

#include <chrono>
#include <memory>
#include <thread>

using namespace zypp;
using namespace zypp::internal;

// A shutdown must cancel a download that is stalled waiting for a reply and surface as
// the same UserRequestException(ABORT) the legacy zypp_poll() backend throws.
BOOST_AUTO_TEST_CASE(shutdown_cancels_stalled_webserver_download)
{
  // A handler that accepts the request and then stays silent for a while.
  WebServer web( ( Pathname( TESTS_SHARED_DIR ) / "data" / "dummywebroot" ).c_str(), 10046 );
  web.addRequestHandler( "stalled", []( WebServer::Request &r ) {
    std::this_thread::sleep_for( std::chrono::milliseconds( 2000 ) );
    r.rout << "Status: 200\r\n"
              "\r\n"
              "Hello";
  } );
  BOOST_REQUIRE( web.start() );

  ZYpp::clearShutdownSignal(); // independent of test order

  Url url( web.url() );
  url.setPathName( "/handler/stalled" );

  zypp::filesystem::TmpFile target;
  auto req = std::make_shared<zyppng::NetworkRequest>( url, target.path() );
  req->transferSettings() = web.transferSettings();

  // Keep the dispatcher alive (ThreadData stores it as a weak_ptr) and arm a timer that
  // requests the shutdown ~200ms into the stalled transfer.
  auto evDispatcher = zyppng::ThreadData::current().ensureDispatcher();
  auto shutdownTimer = zyppng::Timer::create();
  shutdownTimer->setSingleShot( true );
  shutdownTimer->sigExpired().connect( []( const zyppng::Timer & ){ ZYpp::setShutdownSignal(); } );
  shutdownTimer->start( 200 );

  MediaNetworkRequestExecutor executor;
  const auto start = std::chrono::steady_clock::now();
  BOOST_CHECK_THROW( executor.executeRequest( req, nullptr ), UserRequestException );
  const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start ).count();
  BOOST_CHECK_LT( elapsedMs, 5000 ); // far below the request timeout

  ZYpp::clearShutdownSignal();
  web.stop();
}
