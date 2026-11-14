/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\----------------------------------------------------------------------/
*
* Tests for the cancellable Curl2 (ng) download path.
*
* They use a tiny raw TCP server (accept + optional response) so they do not
* depend on the nginx/fcgi test rig (WebServer) which is not available when
* the media backend tests are disabled.
*/
#include <boost/test/unit_test.hpp>

#include <zypp/media/detail/MediaNetworkRequestExecutor.h>
#include <zypp-core/base/UserRequestException>
#include <zypp-core/base/Env.h>

#include <zypp-curl/ng/network/Request>
#include <zypp-core/ng/base/EventLoop>
#include <zypp-core/ng/base/Timer>
#include <zypp-core/ng/base/private/threaddata_p.h>

#include <zypp-core/Url.h>
#include <zypp/TmpPath.h>
#include <zypp/ZYpp.h>
#include <zypp/RepoInfo.h>
#include <zypp-core/OnMediaLocation>
#include <zypp/repo/RepoProvideFile.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace zypp;
using namespace zypp::internal;

namespace {

  /*!
   * Minimal single-connection TCP server.
   *
   * \c Stall accepts the connection, consumes the request and then stays silent
   * (the transfer stalls waiting for a response). \c Respond replies with an empty
   * 200 so the transfer succeeds.
   */
  class TestServer
  {
  public:
    enum class Mode { Stall, Respond };

    explicit TestServer( Mode mode )
      : _mode( mode )
    {
      _listenFd = ::socket( AF_INET, SOCK_STREAM, 0 );
      if ( _listenFd < 0 )
        throw std::runtime_error( "TestServer: socket() failed" );

      int one = 1;
      ::setsockopt( _listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof( one ) );

      sockaddr_in addr {};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
      addr.sin_port = 0;

      socklen_t len = sizeof( addr );
      if ( ::bind( _listenFd, reinterpret_cast<sockaddr *>( &addr ), sizeof( addr ) ) != 0
        || ::listen( _listenFd, 4 ) != 0
        || ::getsockname( _listenFd, reinterpret_cast<sockaddr *>( &addr ), &len ) != 0 ) {
        ::close( _listenFd );
        _listenFd = -1;
        throw std::runtime_error( "TestServer: bind/listen/getsockname failed" );
      }

      _port = ntohs( addr.sin_port );
      _thread = std::thread( [this]{ serve(); } );
    }

    ~TestServer()
    {
      _stop = true;
      if ( _thread.joinable() )
        _thread.join();
      if ( _listenFd >= 0 )
        ::close( _listenFd );
    }

    TestServer( const TestServer & ) = delete;
    TestServer & operator=( const TestServer & ) = delete;

    std::string url( const std::string & path = "/foo" ) const
    { return "http://127.0.0.1:" + std::to_string( _port ) + path; }

    int connections() const
    { return _connections.load(); }

  private:
    void serve()
    {
      while ( !_stop ) {
        pollfd pfd { _listenFd, POLLIN, 0 };
        const int r = ::poll( &pfd, 1, 100 );
        if ( r <= 0 )
          continue;

        const int conn = ::accept( _listenFd, nullptr, nullptr );
        if ( conn < 0 )
          continue;
        ++_connections;

        char buf[4096];
        ::read( conn, buf, sizeof( buf ) ); // consume the request (best effort)

        if ( _mode == Mode::Respond ) {
          const char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
          ::write( conn, resp, ::strlen( resp ) );
          ::close( conn );
        } else {
          // stall: hold the connection open, never reply
          while ( !_stop )
            std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
          ::close( conn );
        }
      }
    }

    Mode                _mode;
    int                 _listenFd = -1;
    uint16_t            _port = 0;
    std::atomic<bool>   _stop{ false };
    std::atomic<int>    _connections{ 0 };
    std::thread         _thread;
  };

  /*!
   * A listening socket with a saturated accept queue. The kernel silently drops the SYN
   * of any further connection, so a connect() to it never completes (it stays in
   * SYN_SENT). This gives a deterministic "stuck in connect" without depending on an
   * externally black-holed address.
   */
  class BlackholeServer
  {
  public:
    BlackholeServer()
    {
      _listenFd = ::socket( AF_INET, SOCK_STREAM, 0 );
      if ( _listenFd < 0 )
        throw std::runtime_error( "BlackholeServer: socket() failed" );

      int one = 1;
      ::setsockopt( _listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof( one ) );

      sockaddr_in addr {};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
      addr.sin_port = 0;

      socklen_t len = sizeof( addr );
      // backlog 1: the accept queue fills with a couple of connections.
      if ( ::bind( _listenFd, reinterpret_cast<sockaddr *>( &addr ), sizeof( addr ) ) != 0
        || ::listen( _listenFd, 1 ) != 0
        || ::getsockname( _listenFd, reinterpret_cast<sockaddr *>( &addr ), &len ) != 0 ) {
        ::close( _listenFd );
        _listenFd = -1;
        throw std::runtime_error( "BlackholeServer: bind/listen/getsockname failed" );
      }

      _port = ntohs( addr.sin_port );

      // Saturate the accept queue. The connects are non-blocking (and kept open) so the
      // ones that do not fit stay pending and hold the queue full.
      for ( int i = 0; i < 32; ++i ) {
        const int conn = ::socket( AF_INET, SOCK_STREAM, 0 );
        if ( conn < 0 )
          continue;
        const int flags = ::fcntl( conn, F_GETFL, 0 );
        ::fcntl( conn, F_SETFL, flags | O_NONBLOCK );
        ::connect( conn, reinterpret_cast<sockaddr *>( &addr ), sizeof( addr ) ); // EINPROGRESS
        _fillers.push_back( conn );
      }
    }

    ~BlackholeServer()
    {
      for ( int conn : _fillers )
        if ( conn >= 0 )
          ::close( conn );
      if ( _listenFd >= 0 )
        ::close( _listenFd );
    }

    BlackholeServer( const BlackholeServer & ) = delete;
    BlackholeServer & operator=( const BlackholeServer & ) = delete;

    std::string url( const std::string & path = "/foo" ) const
    { return "http://127.0.0.1:" + std::to_string( _port ) + path; }

  private:
    int                _listenFd = -1;
    uint16_t           _port = 0;
    std::vector<int>   _fillers;
  };

  // Request a shutdown from the loop thread after \a delayMs.
  zyppng::Timer::Ptr armShutdownTimer( uint64_t delayMs )
  {
    auto timer = zyppng::Timer::create();
    timer->setSingleShot( true );
    timer->sigExpired().connect( []( const zyppng::Timer & ){ ZYpp::setShutdownSignal(); } );
    timer->start( delayMs );
    return timer;
  }

  zyppng::NetworkRequestRef makeRequest( const std::string & url, const Pathname & target )
  { return std::make_shared<zyppng::NetworkRequest>( Url( url ), target ); }

  // Run the request and assert it aborts as a *user* abort; returns true iff it did.
  // This pins the public exception contract: the Curl2 backend must throw the same
  // UserRequestException(ABORT) the legacy zypp_poll() backend throws.
  bool expectUserAbort( MediaNetworkRequestExecutor & executor, zyppng::NetworkRequestRef & req )
  {
    try {
      executor.executeRequest( req, nullptr );
    }
    catch ( const UserRequestException & excpt ) {
      BOOST_CHECK_EQUAL( excpt.kind(), UserRequestException::ABORT );
      return true;
    }
    catch ( ... ) {
      return false;
    }
    return false;
  }

} // anonymous namespace

// A shutdown while the connection is still being established (the peer black-holes the
// SYN) must abort promptly, without waiting for any connect timeout.
BOOST_AUTO_TEST_CASE(shutdown_during_connect_is_prompt)
{
  BlackholeServer server;
  zypp::filesystem::TmpFile target;
  MediaNetworkRequestExecutor executor;

  ZYpp::clearShutdownSignal(); // independent of test order

  auto req = makeRequest( server.url(), target.path() );
  auto shutdownTimer = armShutdownTimer( 300 );

  const auto start = std::chrono::steady_clock::now();
  const bool aborted = expectUserAbort( executor, req );
  const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start ).count();
  BOOST_CHECK( aborted );
  BOOST_CHECK_LT( elapsedMs, 5000 ); // far below the default connect timeout

  ZYpp::clearShutdownSignal();
}

// A shutdown while the connection is established but the server has not sent anything yet
// (waiting for the first reply byte) must abort promptly.
BOOST_AUTO_TEST_CASE(shutdown_while_waiting_for_reply_is_prompt)
{
  TestServer server( TestServer::Mode::Stall );
  zypp::filesystem::TmpFile target;
  MediaNetworkRequestExecutor executor;

  ZYpp::clearShutdownSignal(); // independent of test order

  auto req = makeRequest( server.url(), target.path() );
  auto shutdownTimer = armShutdownTimer( 300 );

  const auto start = std::chrono::steady_clock::now();
  const bool aborted = expectUserAbort( executor, req );
  const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start ).count();
  BOOST_CHECK( aborted );
  BOOST_CHECK_LT( elapsedMs, 5000 ); // far below the default connect timeout
  // The server accepted the connection, so we were past connect, waiting for the reply.
  BOOST_CHECK_GE( server.connections(), 1 );

  ZYpp::clearShutdownSignal();
}

// A shutdown that is already pending aborts before the request is even enqueued.
BOOST_AUTO_TEST_CASE(pending_shutdown_aborts_before_connecting)
{
  TestServer server( TestServer::Mode::Stall );
  zypp::filesystem::TmpFile target;
  MediaNetworkRequestExecutor executor;

  ZYpp::clearShutdownSignal();
  ZYpp::setShutdownSignal();

  auto req = makeRequest( server.url(), target.path() );
  BOOST_CHECK( expectUserAbort( executor, req ) );
  BOOST_CHECK_EQUAL( server.connections(), 0 );

  ZYpp::clearShutdownSignal();
}

// The shutdown stays set until it is cleared: a caller that treats the abort as a soft
// error and moves on must not silently start the next transfer (Ctrl-C stops the command).
BOOST_AUTO_TEST_CASE(shutdown_stays_set_until_cleared)
{
  TestServer server( TestServer::Mode::Stall );
  zypp::filesystem::TmpFile target;
  MediaNetworkRequestExecutor executor;

  ZYpp::clearShutdownSignal();
  ZYpp::setShutdownSignal();

  auto req1 = makeRequest( server.url(), target.path() );
  BOOST_CHECK( expectUserAbort( executor, req1 ) );
  auto req2 = makeRequest( server.url(), target.path() );
  BOOST_CHECK( expectUserAbort( executor, req2 ) );
  BOOST_CHECK_EQUAL( server.connections(), 0 );

  ZYpp::clearShutdownSignal();
}

// After the application clears the signal, transfers work again.
BOOST_AUTO_TEST_CASE(clear_shutdown_lets_a_request_proceed)
{
  TestServer server( TestServer::Mode::Respond );
  zypp::filesystem::TmpFile target;
  MediaNetworkRequestExecutor executor;

  ZYpp::clearShutdownSignal();
  ZYpp::setShutdownSignal();
  ZYpp::clearShutdownSignal();

  auto req = makeRequest( server.url(), target.path() );
  BOOST_CHECK_NO_THROW( executor.executeRequest( req, nullptr ) );
  BOOST_CHECK( !req->hasError() );
}

// The abort has to stay terminal all the way up through RepoProvideFile: it must come
// out as a UserRequestException and must NOT be retried as a transient download error
// (RepoProvideFile.cc logs "Trying next url" for any generic Exception).
BOOST_AUTO_TEST_CASE(shutdown_is_terminal_through_repo_provide_file)
{
  // Pin the Curl2 backend: a build with LIBZYPP_CONFIG_USE_LEGACY_CURL_BACKEND_BY_DEFAULT
  // would otherwise exercise the legacy zypp_poll() path and not cover this change.
  env::ScopedSet curl2Env( "ZYPP_CURL2", "1" );

  TestServer server( TestServer::Mode::Stall );
  zypp::filesystem::TmpDir packageDir;
  zypp::filesystem::TmpDir metaDir;

  RepoInfo repo;
  repo.setAlias( "cancel-test" );
  repo.setBaseUrl( Url( server.url( "/" ) ) );
  repo.setMetadataPath( metaDir.path() );
  repo.setPackagesPath( packageDir.path() );

  ZYpp::clearShutdownSignal(); // independent of test order

  // provideFile() creates the executor (and its event dispatcher) internally, so make
  // sure a dispatcher exists for the thread and keep it alive before arming the timer on it.
  auto evDispatcher = zyppng::ThreadData::current().ensureDispatcher();
  auto shutdownTimer = armShutdownTimer( 300 );

  const auto start = std::chrono::steady_clock::now();
  BOOST_CHECK_THROW( zypp::repo::provideFile( repo, OnMediaLocation( "/cancelme" ) ),
                     UserRequestException );
  const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start ).count();
  BOOST_CHECK_LT( elapsedMs, 5000 ); // far below the default connect timeout
  // The abort happened while the transfer was in flight, not before it started.
  BOOST_CHECK_GE( server.connections(), 1 );

  ZYpp::clearShutdownSignal();
}
