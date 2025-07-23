#include <boost/test/unit_test.hpp>
#include <zypp-core/zyppng/base/EventLoop>
#include <zypp-core/zyppng/base/EventDispatcher>
#include <zypp-core/zyppng/base/Timer>
#include <zypp-core/zyppng/io/AsyncDataSource>
#include <glib-unix.h>

#include <zypp-core/zyppng/rpc/stompframestream.h>
#include <zypp-core/rpc/PluginFrame.h>


BOOST_AUTO_TEST_CASE(ReceiveFrame)
{
  int pipeFds[2] { -1, -1 };
  BOOST_REQUIRE( g_unix_open_pipe( pipeFds, FD_CLOEXEC, nullptr ) );

  auto loop = zyppng::EventLoop::create();
  auto dataSource = zyppng::AsyncDataSource::create();

  BOOST_REQUIRE( dataSource->openFds( { pipeFds[0] } ) );
  BOOST_REQUIRE( dataSource->canRead() );
  BOOST_REQUIRE( dataSource->readFdOpen() );

  std::optional<zypp::PluginFrame> received;

  auto msgQueue = zyppng::StompFrameStream::create ( dataSource );
  msgQueue->connectFunc( &zyppng::StompFrameStream::sigMessageReceived, [&](){
    received = msgQueue->nextMessage ();
    loop->quit();
  });

  msgQueue->connectFunc( &zyppng::StompFrameStream::sigInvalidMessageReceived, [&](){
    loop->quit();
  });

  // make sure we are not stuck
  auto timer = zyppng::Timer::create();
  timer->start( 1000 );
  timer->connectFunc( &zyppng::Timer::sigExpired, [&]( auto & ){
    loop->quit();
  });

  {
    std::string_view text (
          "COMMAND\n"
          "content-length:5\n"
          "\n"
          "Hello\0", 32
    );

    std::thread writer( []( int writeFd, std::string_view text ){
      ::write( writeFd, text.data(), text.length() );
      ::close( writeFd );
    }, pipeFds[1], text );

    loop->run();
    writer.join();
  }

  ::close( pipeFds[0] );

  BOOST_REQUIRE( received.has_value () );
  BOOST_REQUIRE_EQUAL( received->command (), "COMMAND" );
  BOOST_REQUIRE_EQUAL( received->bodyRef ().asStringView(), "Hello" );

}

BOOST_AUTO_TEST_CASE(ReceiveFrameBinary)
{
  int pipeFds[2] { -1, -1 };
  BOOST_REQUIRE( g_unix_open_pipe( pipeFds, FD_CLOEXEC, nullptr ) );

  auto loop = zyppng::EventLoop::create();
  auto dataSource = zyppng::AsyncDataSource::create();

  BOOST_REQUIRE( dataSource->openFds( { pipeFds[0] } ) );
  BOOST_REQUIRE( dataSource->canRead() );
  BOOST_REQUIRE( dataSource->readFdOpen() );

  std::optional<zypp::PluginFrame> received;

  auto msgQueue = zyppng::StompFrameStream::create ( dataSource );
  msgQueue->connectFunc( &zyppng::StompFrameStream::sigMessageReceived, [&](){
    received = msgQueue->nextMessage ();
    loop->quit();
  });

  msgQueue->connectFunc( &zyppng::StompFrameStream::sigInvalidMessageReceived, [&](){
    loop->quit();
  });

  // make sure we are not stuck
  auto timer = zyppng::Timer::create();
  timer->start( 1000 );
  timer->connectFunc( &zyppng::Timer::sigExpired, [&]( auto & ){
    loop->quit();
  });

  {
    std::string_view text (
          "COMMAND\n"
          "content-length:6\n"
          "\n"
          "He\0llo\0", 33
    );

    std::thread writer( []( int writeFd, std::string_view text ){
      ::write( writeFd, text.data(), text.length() );
      ::close( writeFd );
    }, pipeFds[1], text );

    loop->run();
    writer.join();
  }

  ::close( pipeFds[0] );

  BOOST_REQUIRE( received.has_value () );
  BOOST_REQUIRE_EQUAL( received->command (), "COMMAND" );
  BOOST_REQUIRE( received->bodyRef () == zyppng::ByteArray("He\0llo", 6) );

}

BOOST_AUTO_TEST_CASE(ReceiveFrameNoContentLen)
{
  int pipeFds[2] { -1, -1 };
  BOOST_REQUIRE( g_unix_open_pipe( pipeFds, FD_CLOEXEC, nullptr ) );

  auto loop = zyppng::EventLoop::create();
  auto dataSource = zyppng::AsyncDataSource::create();

  BOOST_REQUIRE( dataSource->openFds( { pipeFds[0] } ) );
  BOOST_REQUIRE( dataSource->canRead() );
  BOOST_REQUIRE( dataSource->readFdOpen() );

  std::optional<zypp::PluginFrame> received;

  auto msgQueue = zyppng::StompFrameStream::create ( dataSource );
  msgQueue->connectFunc( &zyppng::StompFrameStream::sigMessageReceived, [&](){
    received = msgQueue->nextMessage ();
    loop->quit();
  });

  msgQueue->connectFunc( &zyppng::StompFrameStream::sigInvalidMessageReceived, [&](){
    loop->quit();
  });

  // make sure we are not stuck
  auto timer = zyppng::Timer::create();
  timer->start( 1000 );
  timer->connectFunc( &zyppng::Timer::sigExpired, [&]( auto & ){
    loop->quit();
  });

  {
    std::string_view text (
          "COMMAND\n"
          "\n"
          "Hello\0", 15
    );

    std::thread writer( []( int writeFd, std::string_view text ){
      ::write( writeFd, text.data(), text.length() );
      ::close( writeFd );
    }, pipeFds[1], text );

    loop->run();
    writer.join();
  }

  ::close( pipeFds[0] );

  BOOST_REQUIRE( received.has_value () );
  BOOST_REQUIRE_EQUAL( received->command (), "COMMAND" );
  BOOST_REQUIRE_EQUAL( received->bodyRef ().asStringView(), "Hello" );

}

BOOST_AUTO_TEST_CASE(ReceiveMultipleFrames)
{
  int pipeFds[2] { -1, -1 };
  BOOST_REQUIRE( g_unix_open_pipe( pipeFds, FD_CLOEXEC, nullptr ) );

  auto loop = zyppng::EventLoop::create();
  auto dataSource = zyppng::AsyncDataSource::create();

  BOOST_REQUIRE( dataSource->openFds( { pipeFds[0] } ) );
  BOOST_REQUIRE( dataSource->canRead() );
  BOOST_REQUIRE( dataSource->readFdOpen() );

  bool receivedErr = false;
  bool timedOut    = false;
  std::vector<zypp::PluginFrame> received;

  auto msgQueue = zyppng::StompFrameStream::create ( dataSource );
  msgQueue->connectFunc( &zyppng::StompFrameStream::sigMessageReceived, [&](){
    received.push_back(*msgQueue->nextMessage());
  });

  msgQueue->connectFunc( &zyppng::StompFrameStream::sigInvalidMessageReceived, [&](){
    receivedErr = true;
    loop->quit();
  });

  dataSource->connectFunc( &zyppng::AsyncDataSource::sigReadChannelFinished, [&]( uint channel ){
    if ( channel == 0 )
      loop->quit();
  });

  // make sure we are not stuck
  auto timer = zyppng::Timer::create();
  timer->start( 1000 );
  timer->connectFunc( &zyppng::Timer::sigExpired, [&]( auto & ){
    timedOut = true;
    loop->quit();
  });

  {
    std::string_view text (
          "COMMAND\n"
          "content-length:5\n"
          "\n"
          "Hello\0", 32
    );

    std::thread writer( []( int writeFd, std::string_view text ){
      ::write( writeFd, text.data(), text.length() );
      ::write( writeFd, text.data(), text.length() );
      ::write( writeFd, text.data(), text.length() );
      ::close( writeFd );
    }, pipeFds[1], text );

    loop->run();
    writer.join();
  }

  ::close( pipeFds[0] );

  BOOST_REQUIRE( !timedOut );
  BOOST_REQUIRE( !receivedErr);
  BOOST_REQUIRE_EQUAL( received.size (), 3 );

  for( uint i = 0; i < received.size(); i++ ) {
    BOOST_REQUIRE_EQUAL( received[i].command (), "COMMAND" );
    BOOST_REQUIRE_EQUAL( received[i].bodyRef ().asStringView(), "Hello" );
  }

}

BOOST_AUTO_TEST_CASE(InvalidFrame)
{
  int pipeFds[2] { -1, -1 };
  BOOST_REQUIRE( g_unix_open_pipe( pipeFds, FD_CLOEXEC, nullptr ) );

  auto loop = zyppng::EventLoop::create();
  auto dataSource = zyppng::AsyncDataSource::create();

  BOOST_REQUIRE( dataSource->openFds( { pipeFds[0] } ) );
  BOOST_REQUIRE( dataSource->canRead() );
  BOOST_REQUIRE( dataSource->readFdOpen() );

  std::optional<zypp::PluginFrame> received;
  bool receivedErr = false;
  bool timedOut    = false;

  auto msgQueue = zyppng::StompFrameStream::create ( dataSource );
  msgQueue->connectFunc( &zyppng::StompFrameStream::sigMessageReceived, [&](){
    received = msgQueue->nextMessage ();
    loop->quit();
  });

  msgQueue->connectFunc( &zyppng::StompFrameStream::sigInvalidMessageReceived, [&](){
    receivedErr = true;
    loop->quit();
  });

  // make sure we are not stuck
  auto timer = zyppng::Timer::create();
  timer->start( 1000 );
  timer->connectFunc( &zyppng::Timer::sigExpired, [&]( auto & ){
    timedOut = true;
    loop->quit();
  });

  {
    std::string_view text (
          "COMMAND\n"
          "content-length-5\n" //wrong header, colon is missing
          "\n"
          "Hello\0", 32
    );

    std::thread writer( []( int writeFd, std::string_view text ){
      ::write( writeFd, text.data(), text.length() );
      ::close( writeFd );
    }, pipeFds[1], text );

    loop->run();
    writer.join();
  }

  ::close( pipeFds[0] );

  BOOST_REQUIRE( !received.has_value () );
  BOOST_REQUIRE( !timedOut );
  BOOST_REQUIRE( receivedErr );

}

BOOST_AUTO_TEST_CASE(BodyNotTerminated)
{
  int pipeFds[2] { -1, -1 };
  BOOST_REQUIRE( g_unix_open_pipe( pipeFds, FD_CLOEXEC, nullptr ) );

  auto loop = zyppng::EventLoop::create();
  auto dataSource = zyppng::AsyncDataSource::create();

  BOOST_REQUIRE( dataSource->openFds( { pipeFds[0] } ) );
  BOOST_REQUIRE( dataSource->canRead() );
  BOOST_REQUIRE( dataSource->readFdOpen() );

  std::optional<zypp::PluginFrame> received;
  bool receivedErr = false;
  bool timedOut    = false;

  auto msgQueue = zyppng::StompFrameStream::create ( dataSource );
  msgQueue->connectFunc( &zyppng::StompFrameStream::sigMessageReceived, [&](){
    received = msgQueue->nextMessage ();
    loop->quit();
  });

  msgQueue->connectFunc( &zyppng::StompFrameStream::sigInvalidMessageReceived, [&](){
    receivedErr = true;
    loop->quit();
  });

  // make sure we are not stuck
  auto timer = zyppng::Timer::create();
  timer->start( 1000 );
  timer->connectFunc( &zyppng::Timer::sigExpired, [&]( auto & ){
    timedOut = true;
    loop->quit();
  });

  {
    std::string_view text (
          "COMMAND\n"
          "content-length:5\n" //wrong header, colon is missing
          "\n"
          "Hello", 31
    );

    std::thread writer( []( int writeFd, std::string_view text ){
      ::write( writeFd, text.data(), text.length() );
      ::write( writeFd, text.data(), text.length() );
      ::close( writeFd );
    }, pipeFds[1], text );

    loop->run();
    writer.join();
  }

  ::close( pipeFds[0] );

  BOOST_REQUIRE( !received.has_value () );
  BOOST_REQUIRE( !timedOut );
  BOOST_REQUIRE( receivedErr );

}


BOOST_AUTO_TEST_CASE(SerializeFrame)
{
  int pipeFds[2] { -1, -1 };
  BOOST_REQUIRE( g_unix_open_pipe( pipeFds, FD_CLOEXEC, nullptr ) );

  auto loop = zyppng::EventLoop::create();
  auto dataSourceRead = zyppng::AsyncDataSource::create();
  auto dataSourceWrite = zyppng::AsyncDataSource::create();

  BOOST_REQUIRE( dataSourceRead->openFds( { pipeFds[0] } ) );
  BOOST_REQUIRE( dataSourceRead->canRead() );
  BOOST_REQUIRE( dataSourceRead->readFdOpen() );

  BOOST_REQUIRE( dataSourceWrite->openFds( {}, pipeFds[1] ) );
  BOOST_REQUIRE( dataSourceWrite->canWrite() );

  zyppng::IODeviceOStreamBuf streamBuf( dataSourceWrite );
  std::ostream outStr( &streamBuf );

  zypp::PluginFrame data("COMMAND");
  data.addHeader ("header1", "value");
  data.addHeader ("header2", "value");
  data.addHeader ("header3", "value");
  data.setBody   ("Somedata");

  bool gotBytesWritten = false;
  dataSourceWrite->connectFunc( &zyppng::IODevice::sigAllBytesWritten, [&](){
    gotBytesWritten = true;
  });

  BOOST_CHECK_NO_THROW(data.writeTo( outStr ));

  std::optional<zypp::PluginFrame> received;
  bool receivedErr = false;
  bool timedOut    = false;

  auto msgQueue = zyppng::StompFrameStream::create ( dataSourceRead );
  msgQueue->connectFunc( &zyppng::StompFrameStream::sigMessageReceived, [&](){
    received = msgQueue->nextMessage ();
    loop->quit();
  });

  msgQueue->connectFunc( &zyppng::StompFrameStream::sigInvalidMessageReceived, [&](){
    receivedErr = true;
    loop->quit();
  });

  // make sure we are not stuck
  auto timer = zyppng::Timer::create();
  timer->start( 1000 );
  timer->connectFunc( &zyppng::Timer::sigExpired, [&]( auto & ){
    timedOut = true;
    loop->quit();
  });

  loop->run();

  ::close( pipeFds[0] );

  BOOST_REQUIRE( !timedOut );
  BOOST_REQUIRE( !receivedErr );
  BOOST_REQUIRE( received.has_value () );
  BOOST_REQUIRE_EQUAL( received->command(), "COMMAND" );
  BOOST_REQUIRE_EQUAL( received->headerSize(), 3 );
  BOOST_REQUIRE_EQUAL( received->getHeader("header1", {}), "value" );
  BOOST_REQUIRE_EQUAL( received->getHeader("header2", {}), "value" );
  BOOST_REQUIRE_EQUAL( received->getHeader("header3", {}), "value" );
  BOOST_REQUIRE_EQUAL( received->bodyRef().asStringView(), "Somedata");

}
