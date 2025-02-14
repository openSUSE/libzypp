#include "WebServer.h"
#include <zypp-core/fs/TmpPath.h>
#include <zypp/ZYppCallbacks.h>
#include <zypp/ZConfig.h>

#include <zypp/media/MediaManager.h>
#include <zypp-core/Url.h>
#include <zypp/Digest.h>

#include <iostream>

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

namespace bdata = boost::unit_test::data;

// Globals can be broken in tests, better manually create the byte objects
const auto makeBytes( zypp::ByteCount::SizeType size ) {
  return zypp::ByteCount( size, zypp::ByteCount::Unit( 1LL, "B", 0 ) );
};

const auto makeKBytes( zypp::ByteCount::SizeType size ) {
  return zypp::ByteCount( size, zypp::ByteCount::Unit( 1024LL, "KiB", 1 ) );
};

bool withSSL[]{ true, false};
std::vector<std::string> backend = { "curl", "curl2", "multicurl" };

BOOST_DATA_TEST_CASE( base_provide_zck, bdata::make( withSSL ) * bdata::make( backend ), withSSL, backend )
{

  int primaryRequests = 0;

  zypp::Pathname testRoot = zypp::Pathname(TESTS_SRC_DIR)/"zyppng/data/downloader";
  WebServer web( testRoot.c_str(), 10001, withSSL );
  BOOST_REQUIRE( web.start() );

  web.addRequestHandler("primary", [ & ]( WebServer::Request &req ){
    primaryRequests++;
    req.rout << "Location: /primary.xml.zck\r\n\r\n";
    return;
  });

  zypp::media::MediaManager   mm;
  zypp::media::MediaAccessId  id;

  zypp::Url mediaUrl = web.url();
  mediaUrl.setQueryParam( "mediahandler", backend );

  if( withSSL ) {
    mediaUrl.setQueryParam("ssl_capath", web.caPath().asString() );
  }

  BOOST_CHECK_NO_THROW( id = mm.open( mediaUrl ) );
  BOOST_CHECK_NO_THROW( mm.attach(id) );

  zypp::OnMediaLocation loc("/handler/primary");
  loc.setDeltafile( testRoot/"primary-deltatemplate.xml.zck" );
  loc.setDownloadSize( makeBytes(274638) );
  loc.setHeaderSize( makeBytes(11717) );
  loc.setHeaderChecksum( zypp::CheckSum( zypp::Digest::sha256(), "90a1a1b99ba3b6c8ae9f14b0c8b8c43141c69ec3388bfa3b9915fbeea03926b7") );

  BOOST_CHECK_NO_THROW( mm.provideFile( id, loc ) );

  // if zck is enabled, we have 2 requests, otherwise its just a normal provide
#ifdef ENABLE_ZCHUNK_COMPRESSION
  if ( backend == "curl2" )
    BOOST_REQUIRE_EQUAL( primaryRequests, 2 ); // header + chunks
  else
#endif
    BOOST_REQUIRE_EQUAL( primaryRequests, 1 );
}

//case: file with authentication
BOOST_DATA_TEST_CASE( base_provide_auth, bdata::make( withSSL ) * bdata::make( backend ), withSSL, backend )
{
  //don't write or read creds from real settings dir
  zypp::filesystem::TmpDir repoManagerRoot;
  zypp::ZConfig::instance().setRepoManagerRoot( repoManagerRoot.path() );

  zypp::Pathname testRoot = zypp::Pathname(TESTS_SRC_DIR)/"zyppng/data/downloader";
  WebServer web( testRoot.c_str(), 10001, withSSL );
  BOOST_REQUIRE( web.start() );

  web.addRequestHandler("primary", WebServer::makeBasicAuthHandler("Basic dGVzdDp0ZXN0", []( WebServer::Request &req ){
    req.rout << "Status: 307 Temporary Redirect\r\n"
    "Location: /primary.xml.zck\r\n\r\n";
  }));

  zypp::media::MediaManager   mm;
  zypp::media::MediaAccessId  id;

  zypp::Url mediaUrl = web.url();
  mediaUrl.setQueryParam( "mediahandler", backend );

  if( withSSL ) {
    mediaUrl.setQueryParam("ssl_capath", web.caPath().asString() );
  }

  BOOST_CHECK_NO_THROW( id = mm.open( mediaUrl ) );
  BOOST_CHECK_NO_THROW( mm.attach(id) );

  zypp::OnMediaLocation loc("/handler/primary");
  loc.setDeltafile( testRoot/"primary-deltatemplate.xml.zck" );
  loc.setDownloadSize( makeBytes(274638) );
  loc.setHeaderSize( makeBytes(11717) );
  loc.setHeaderChecksum( zypp::CheckSum( zypp::Digest::sha256(), "90a1a1b99ba3b6c8ae9f14b0c8b8c43141c69ec3388bfa3b9915fbeea03926b7") );

  {
    // no auth given
    BOOST_REQUIRE_THROW( mm.provideFile( id, loc ), zypp::media::MediaUnauthorizedException );
  }

  {
    //case: user provides wrong auth, then cancels
    struct AuthenticationReportReceiver : public zypp::callback::ReceiveReport<zypp::media::AuthenticationReport>
    {
      int reqCount = 0;
      bool gotAuth = false;
      AuthenticationReportReceiver( ){ connect(); }

      bool prompt(const zypp::Url &,
                  const std::string &,
                  zypp::media::AuthData & auth_data) override {
        if ( reqCount > 1 )
          return false;
        auth_data.setUsername("wrong");
        auth_data.setPassword("auth");
        gotAuth = true;
        reqCount++;
        return true;
      }
    };

    AuthenticationReportReceiver authHandler;
    BOOST_REQUIRE_THROW( mm.provideFile( id, loc ), zypp::media::MediaUnauthorizedException );
    BOOST_REQUIRE_EQUAL( authHandler.gotAuth, true );
  }

  {
    //case: user cancels auth
    struct AuthenticationReportReceiver : public zypp::callback::ReceiveReport<zypp::media::AuthenticationReport>
    {
      bool gotAuth = false;
      AuthenticationReportReceiver( ){ connect(); }

      bool prompt(const zypp::Url &,
                  const std::string &,
                  zypp::media::AuthData &) override  {
        gotAuth = true;
        return false;
      }
    };

    AuthenticationReportReceiver authHandler;
    BOOST_REQUIRE_THROW( mm.provideFile( id, loc ), zypp::media::MediaUnauthorizedException );
    BOOST_REQUIRE_EQUAL( authHandler.gotAuth, true );
  }

  {
    // case: User first gives wrong, then correct auth information
    struct AuthenticationReportReceiver : public zypp::callback::ReceiveReport<zypp::media::AuthenticationReport>
    {
      bool gotAuth = false;
      int reqCount = 0;
      AuthenticationReportReceiver( ){ connect(); }

      bool prompt(const zypp::Url &,
                  const std::string &,
                  zypp::media::AuthData &auth_data) override {
        if ( reqCount >= 1 ) {
          auth_data.setUsername("test");
          auth_data.setPassword("test");
        } else {
          auth_data.setUsername("wrong");
          auth_data.setPassword("auth");
        }
        reqCount++;
        gotAuth = true;
        return true;
      }
    };

    AuthenticationReportReceiver authHandler;
    BOOST_REQUIRE_NO_THROW( mm.provideFile( id, loc ) );
    BOOST_REQUIRE_EQUAL( authHandler.gotAuth, true );
  }

}

// case: requesting a non existant file
BOOST_DATA_TEST_CASE( base_provide_not_found, bdata::make( withSSL ) * bdata::make( backend ), withSSL, backend )
{
  //don't write or read creds from real settings dir
  zypp::filesystem::TmpDir repoManagerRoot;
  zypp::ZConfig::instance().setRepoManagerRoot( repoManagerRoot.path() );

  zypp::Pathname testRoot = zypp::Pathname(TESTS_SRC_DIR)/"zyppng/data/downloader";
  WebServer web( testRoot.c_str(), 10001, withSSL );
  BOOST_REQUIRE( web.start() );

  zypp::media::MediaManager   mm;
  zypp::media::MediaAccessId  id;

  zypp::Url mediaUrl = web.url();
  mediaUrl.setQueryParam( "mediahandler", backend );

  if( withSSL ) {
    mediaUrl.setQueryParam("ssl_capath", web.caPath().asString() );
  }

  BOOST_CHECK_NO_THROW( id = mm.open( mediaUrl ) );
  BOOST_CHECK_NO_THROW( mm.attach(id) );

  zypp::OnMediaLocation loc("/file-not-there.txt");
  {
    // no auth given
    BOOST_REQUIRE_THROW( mm.provideFile( id, loc ), zypp::media::MediaFileNotFoundException );
  }
}

