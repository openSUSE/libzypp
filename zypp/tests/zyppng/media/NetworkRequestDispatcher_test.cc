#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <zypp-core/zyppng/base/EventLoop>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-curl/parser/MetaLinkParser> // for hexstr2bytes
#include <zypp-curl/ng/network/Request>
#include <zypp-curl/ng/network/NetworkRequestDispatcher>
#include <zypp-curl/ng/network/NetworkRequestError>
#include <zypp/TmpPath.h>
#include <zypp/base/String.h>
#include <zypp/Digest.h>
#include <zypp/PathInfo.h>

#include <iostream>
#include <thread>
#include <chrono>

#include "WebServer.h"
#include "FtpServer.h"
#include "TestTools.h"


#define BOOST_TEST_REQ_ERR(REQ, EXPECERR) \
  do { \
  BOOST_REQUIRE( REQ->hasError() ); \
  BOOST_REQUIRE( REQ->error().isError() ); \
  BOOST_REQUIRE_EQUAL( REQ->error().type(), EXPECERR ); \
  } while(false)

#define BOOST_TEST_REQ_SUCCESS(REQ) \
  do { \
  if ( REQ->hasError() ) \
    BOOST_ERROR( REQ->error().toString() ); \
  BOOST_REQUIRE( !REQ->hasError() ); \
  BOOST_REQUIRE( !REQ->error().isError() ); \
  BOOST_REQUIRE_EQUAL( REQ->error().type(), zyppng::NetworkRequestError::NoError ); \
  } while(false)

namespace bdata = boost::unit_test::data;

const char * err404 = "Status: 404 Not Found\r\n"
                     "Date: Tue, 21 May 2019 08:30:59 GMT\r\n"
                     "Server: Apache/2.4.23 (Linux/SUSE)\r\n"
                     "X-Prefix: 93.192.0.0/10\r\n"
                     "X-AS: 3320\r\n"
                     "Vary: accept-language,accept-charset"
                     "Accept-Ranges: bytes"
                     "Transfer-Encoding: chunked"
                     "Content-Type: text/html; charset=utf-8"
                     "Content-Language: en\r\n"
                     "\r\n"
                     "Resource is no longer available!";

const char * err401 = "Status: 401 Unauthorized\r\n"
                     "Content-Type: text/html; charset=utf-8\r\n"
                     "WWW-Authenticate: Basic realm=\"User Visible Realm\", charset=\"UTF-8\" \r\n"
                     "\r\n"
                     "Sorry you are not authorized.";

const char * resp204 = "Status: 204\r\n"
                       "server: Apache\r\n"
                       "content-type: text/html;charset=UTF-8\r\n"
                       "date: Wed, 30 Nov 2022 14:31:22 GMT\r\n\r\n";

bool withSSL[] = { true, false };

BOOST_DATA_TEST_CASE(nwdispatcher_basic, bdata::make( withSSL ), withSSL)
{
  std::string dummyContent = "This is just some dummy content,\nto test downloading and signals.";

  auto ev = zyppng::EventLoop::create();

  WebServer web((zypp::Pathname(TESTS_SRC_DIR)/"data"/"dummywebroot").c_str(), 10001, withSSL );
  web.addRequestHandler("getData", WebServer::makeResponse("200 OK", dummyContent ) );
  BOOST_REQUIRE( web.start() );

  BOOST_REQUIRE( !web.isStopped() );

  zyppng::TransferSettings set = web.transferSettings();
  auto disp = std::make_shared<zyppng::NetworkRequestDispatcher>();
  disp->run();
  disp->sigQueueFinished().connect( [&ev]( const zyppng::NetworkRequestDispatcher& ){
    ev->quit();
  });

  bool gotStarted = false;
  bool gotFinished = false;
  bool gotProgress = false;
  off_t lastProgress = 0;
  off_t totalDL = 0;

  zypp::filesystem::TmpFile targetFile;
  zyppng::Url weburl (web.url());
  weburl.setPathName("/handler/getData");

  zyppng::NetworkRequest::Ptr reqData = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  reqData->transferSettings() = set;
  reqData->sigStarted().connect( [ &gotStarted ]( zyppng::NetworkRequest& ){
    gotStarted = true;
  });
  reqData->sigFinished().connect( [ &gotFinished ]( zyppng::NetworkRequest&, const zyppng::NetworkRequestError & ){
    gotFinished = true;
  });
  reqData->sigProgress().connect( [ & ]( zyppng::NetworkRequest &, off_t dltotal, off_t dlnow, off_t, off_t ){
    gotProgress= true;
    lastProgress = dlnow;
    totalDL = dltotal;
  });

  disp->enqueue( reqData );
  if ( disp->count () ) ev->run();

  BOOST_TEST_REQ_SUCCESS( reqData );
  BOOST_REQUIRE( gotStarted );
  BOOST_REQUIRE( gotFinished );
  BOOST_REQUIRE( gotProgress );
  BOOST_REQUIRE_EQUAL( totalDL, dummyContent.length() );
  BOOST_REQUIRE_EQUAL( lastProgress, dummyContent.length() );
}

BOOST_DATA_TEST_CASE(nwdispatcher_http_errors, bdata::make( withSSL ), withSSL)
{
  auto makeErrorResponder = [] ( std::string err ) -> WebServer::RequestHandler  {
    return WebServer::makeResponse( err, "This is a error." );
  };

  auto ev = zyppng::EventLoop::create();
  WebServer web((zypp::Pathname(TESTS_SRC_DIR)/"data"/"dummywebroot").c_str(), 10001, withSSL );

  web.addRequestHandler("get404", WebServer::makeResponse( err404 ) );
  web.addRequestHandler("get401", WebServer::makeResponse( err401 ) );
  web.addRequestHandler("get502", makeErrorResponder( "502 Bad Gateway" ) );
  web.addRequestHandler("get503", makeErrorResponder( "503 Service Unavailable" ) );
  web.addRequestHandler("get504", makeErrorResponder( "504 Gateway Timeout" ) );
  web.addRequestHandler("get403", makeErrorResponder( "403 Forbidden" ) );
  web.addRequestHandler("get410", makeErrorResponder( "410 Gone" ) );
  web.addRequestHandler("get418", makeErrorResponder( "418 I'm a teapot" ) );
  web.addRequestHandler("get204", WebServer::makeResponse( resp204 ) );
  BOOST_REQUIRE( web.start() );

  zyppng::TransferSettings set = web.transferSettings();

  auto disp = std::make_shared<zyppng::NetworkRequestDispatcher>();
  disp->sigQueueFinished().connect( [&ev]( const zyppng::NetworkRequestDispatcher& ){
    ev->quit();
  });

  zyppng::Url weburl (web.url());
  weburl.setPathName("/handler/get404");

  zypp::filesystem::TmpFile targetFile;
  zyppng::NetworkRequest::Ptr req404 = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  req404->transferSettings() = set;
  disp->enqueue( req404 );

  weburl = zyppng::Url( "bad://127.0.0.1" );
  BOOST_REQUIRE( !disp->supportsProtocol(weburl) );
  zyppng::NetworkRequest::Ptr reqInvProto = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  reqInvProto->transferSettings() = set;
  disp->enqueue( reqInvProto );

  weburl = zyppng::Url( web.url() );
  weburl.setPathName("/handler/get401");
  zyppng::NetworkRequest::Ptr reqUnauthorized = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  reqUnauthorized->transferSettings() = set;
  disp->enqueue( reqUnauthorized );

  weburl = zyppng::Url( web.url() );
  weburl.setPathName("/handler/get401");
  zyppng::NetworkRequest::Ptr reqAuthFailed = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  reqAuthFailed->transferSettings() = set;
  reqAuthFailed->transferSettings().setUsername("test");
  reqAuthFailed->transferSettings().setPassword("test");
  disp->enqueue( reqAuthFailed );

  weburl = zyppng::Url( web.url() );
  weburl.setPathName("/handler/get502");
  zyppng::NetworkRequest::Ptr req502 = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  req502->transferSettings() = set;
  disp->enqueue( req502 );

  weburl = zyppng::Url( web.url() );
  weburl.setPathName("/handler/get503");
  zyppng::NetworkRequest::Ptr req503 = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  req503->transferSettings() = set;
  disp->enqueue( req503 );

  weburl = zyppng::Url( web.url() );
  weburl.setPathName("/handler/get504");
  zyppng::NetworkRequest::Ptr req504 = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  req504->transferSettings() = set;
  disp->enqueue( req504 );

  weburl = zyppng::Url( web.url() );
  weburl.setPathName("/handler/get403");
  zyppng::NetworkRequest::Ptr req403 = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  req403->transferSettings() = set;
  disp->enqueue( req403 );

  weburl = zyppng::Url( web.url() );
  weburl.setPathName("/handler/get410");
  zyppng::NetworkRequest::Ptr req410 = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  req410->transferSettings() = set;
  disp->enqueue( req410 );

  weburl = zyppng::Url( web.url() );
  weburl.setPathName("/handler/get418");
  zyppng::NetworkRequest::Ptr req418 = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  req418->transferSettings() = set;
  disp->enqueue( req418 );

  weburl = zyppng::Url( web.url() );
  weburl.setPathName("/handler/get204");
  zyppng::NetworkRequest::Ptr get204 = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  get204->transferSettings() = set;
  disp->enqueue( get204 );

  disp->run();
  if ( disp->count () ) ev->run();

  BOOST_TEST_REQ_ERR( req404, zyppng::NetworkRequestError::NotFound );
  BOOST_TEST_REQ_ERR( reqInvProto, zyppng::NetworkRequestError::UnsupportedProtocol );
  BOOST_TEST_REQ_ERR( reqUnauthorized, zyppng::NetworkRequestError::Unauthorized );
  BOOST_TEST_REQ_ERR( reqAuthFailed, zyppng::NetworkRequestError::AuthFailed );
  BOOST_TEST_REQ_ERR( req502, zyppng::NetworkRequestError::TemporaryProblem );
  BOOST_TEST_REQ_ERR( req503, zyppng::NetworkRequestError::TemporaryProblem );
  BOOST_TEST_REQ_ERR( req504, zyppng::NetworkRequestError::Timeout );
  BOOST_TEST_REQ_ERR( req403, zyppng::NetworkRequestError::Forbidden );
  BOOST_TEST_REQ_ERR( req410, zyppng::NetworkRequestError::NotFound );
  BOOST_TEST_REQ_ERR( req418, zyppng::NetworkRequestError::ServerReturnedError );
  BOOST_TEST_REQ_SUCCESS( get204 );
}

template <typename Server>
void nwdispatcher_download( bool withSSL )
{
  auto ev = zyppng::EventLoop::create();
  auto disp = std::make_shared<zyppng::NetworkRequestDispatcher>();
  disp->sigQueueFinished().connect( [&ev]( const zyppng::NetworkRequestDispatcher& ){
    ev->quit();
  });
  //start request dispatching, does not need to have requests enqueued
  disp->run();

  Server srv((zypp::Pathname(TESTS_SRC_DIR)/"zypp/data/Fetcher/remote-site").c_str(), 10001, withSSL );
  BOOST_REQUIRE( srv.start() );

  zyppng::TransferSettings set = srv.transferSettings();

  zyppng::Url weburl (srv.url());
  weburl.setPathName("/complexdir/subdir1/subdir1-file1.txt");

  const auto &checkFileExists = []( const zypp::filesystem::Pathname &filePath ) -> bool {
    zypp::filesystem::PathInfo targetFileInfo( filePath );
    return ( targetFileInfo.isExist() && targetFileInfo.isFile() );
  };

  const auto &checkFilesum = []( const zypp::filesystem::Pathname &filePath, const zypp::CheckSum sum  ) -> bool {
    std::string fileSum = zypp::filesystem::checksum( filePath, sum.type() );
    fileSum = zypp::str::trim( fileSum );
    const std::string &expSumStr = sum.checksum ();
    return (expSumStr == fileSum);
  };


  // download a full file using a open range starting from 0 with expected checksum
  {
    zypp::filesystem::TmpFile targetFile;
    zyppng::NetworkRequest::Ptr reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
    std::optional<zypp::Digest> dig = zypp::Digest();
    BOOST_REQUIRE_MESSAGE( dig->create( zypp::Digest::sha1() ), "Unable to create Digest " );

    reqDLFile->transferSettings() = set;
    reqDLFile->addRequestRange(0, 0, std::move(dig), zypp::media::hexstr2bytes("f1d2d2f924e986ac86fdf7b36c94bcdf32beec15") );
    disp->enqueue( reqDLFile );
    if ( disp->count () ) ev->run();
    BOOST_TEST_REQ_SUCCESS( reqDLFile );

    BOOST_REQUIRE( checkFileExists(targetFile.path()) );
    BOOST_REQUIRE( checkFilesum(targetFile.path(), zypp::CheckSum::sha1("f1d2d2f924e986ac86fdf7b36c94bcdf32beec15")) );
  }

  // download a full file with expected checksum
  {
    zypp::filesystem::TmpFile targetFile;
    zyppng::NetworkRequest::Ptr reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
    reqDLFile->transferSettings() = set;
    BOOST_REQUIRE_MESSAGE( reqDLFile->setExpectedFileChecksum( zypp::CheckSum::sha1("f1d2d2f924e986ac86fdf7b36c94bcdf32beec15") ), "Unable to set expected checksum" );
    disp->enqueue( reqDLFile );
    if ( disp->count () ) ev->run();
    BOOST_TEST_REQ_SUCCESS( reqDLFile );

    BOOST_REQUIRE( checkFileExists(targetFile.path()) );
    BOOST_REQUIRE( checkFilesum(targetFile.path(), zypp::CheckSum::sha1("f1d2d2f924e986ac86fdf7b36c94bcdf32beec15")) );
  }

  // download a full file using a open range starting from 0 but checksum should fail
  {
    zypp::filesystem::TmpFile targetFile;
    zyppng::NetworkRequest::Ptr reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
    std::optional<zypp::Digest> dig = zypp::Digest();
    BOOST_REQUIRE_MESSAGE( dig->create( zypp::Digest::sha1() ), "Unable to create Digest " );
    //modify the checksum -> request should fail now
    reqDLFile->addRequestRange(0, 0, std::move(dig), zypp::media::hexstr2bytes("f1d2d2f924e986ac86fdf7b36c94bcdf32beec20") );
    disp->enqueue( reqDLFile );
    if ( disp->count () ) ev->run();
    BOOST_TEST_REQ_ERR( reqDLFile, zyppng::NetworkRequestError::InvalidChecksum );

    BOOST_REQUIRE( checkFileExists(targetFile.path()) );
    BOOST_REQUIRE( !checkFilesum(targetFile.path(), zypp::CheckSum::sha1("f1d2d2f924e986ac86fdf7b36c94bcdf32beec20")) );
  }

  // download a full file but checksum should fail
  {
    zypp::filesystem::TmpFile targetFile;
    zyppng::NetworkRequest::Ptr reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
    reqDLFile->transferSettings() = set;
    BOOST_REQUIRE_MESSAGE( reqDLFile->setExpectedFileChecksum( zypp::CheckSum::sha1( "f1d2d2f924e986ac86fdf7b36c94bcdf32beec20") ) , "Unable to set expected checksum" );
    disp->enqueue( reqDLFile );
    if ( disp->count () ) ev->run();
    BOOST_TEST_REQ_ERR( reqDLFile, zyppng::NetworkRequestError::InvalidChecksum );

    BOOST_REQUIRE( checkFileExists(targetFile.path()) );
    BOOST_REQUIRE( !checkFilesum(targetFile.path(), zypp::CheckSum::sha1( "f1d2d2f924e986ac86fdf7b36c94bcdf32beec20")) );
  }

  {
    zypp::filesystem::TmpFile targetFile;
    zyppng::NetworkRequest::Ptr reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
    weburl = srv.url();
    weburl.setPathName("/file-1.txt");
    reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
    reqDLFile->transferSettings() = set;
    reqDLFile->setUrl( weburl );
    reqDLFile->addRequestRange( 0, 7 );
    disp->enqueue( reqDLFile );
    if ( disp->count () ) ev->run();
    BOOST_TEST_REQ_SUCCESS( reqDLFile );

    BOOST_REQUIRE( checkFileExists(targetFile.path()) );
    BOOST_REQUIRE( checkFilesum(targetFile.path(), zypp::CheckSum::md5(std::string("16d2b386b2034b9488996466aaae0b57"))) );
  }
}

BOOST_DATA_TEST_CASE(nwdispatcher_http_download, bdata::make( withSSL ), withSSL )
{
  nwdispatcher_download<WebServer>(withSSL);
}

BOOST_DATA_TEST_CASE(nwdispatcher_ftp_download, bdata::make( withSSL ), withSSL )
{
  nwdispatcher_download<FtpServer>(withSSL);
}

BOOST_DATA_TEST_CASE(nwdispatcher_delay_download, bdata::make( withSSL ), withSSL )
{
  auto ev = zyppng::EventLoop::create();
  auto disp = std::make_shared<zyppng::NetworkRequestDispatcher>();
  disp->sigQueueFinished().connect( [&ev]( const zyppng::NetworkRequestDispatcher& ){
    ev->quit();
  });

  disp->run();

  WebServer web((zypp::Pathname(TESTS_SRC_DIR)/"data"/"dummywebroot").c_str(), 10001, withSSL );

  web.addRequestHandler("stalled", []( WebServer::Request &r ){
    std::this_thread::sleep_for( std::chrono::milliseconds ( 2000 ) );
    r.rout << "Status: 200\r\n"
              "\r\n"
              "Hello";
  });

  BOOST_REQUIRE( web.start() );

  zyppng::TransferSettings set = web.transferSettings();
  set.setTimeout( 1 );

  zypp::filesystem::TmpFile targetFile;

  zyppng::Url weburl (web.url());
  weburl.setPathName("/handler/stalled");

  zyppng::NetworkRequest::Ptr reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  reqDLFile->transferSettings() = set;

  disp->enqueue( reqDLFile );
  if ( disp->count () ) ev->run();

  BOOST_TEST_REQ_ERR( reqDLFile, zyppng::NetworkRequestError::Timeout );
}

template <typename Server>
void nwdispatcher_multipart_dl_impl( bool withSSL ) {
  auto ev = zyppng::EventLoop::create();
  auto disp = std::make_shared<zyppng::NetworkRequestDispatcher>();
  disp->sigQueueFinished().connect( [&ev]( const zyppng::NetworkRequestDispatcher& ){
    ev->quit();
  });

  disp->run();

  Server web((zypp::Pathname(TESTS_SRC_DIR)/"zypp/data/Fetcher/remote-site").c_str(), 10001, withSSL );
  BOOST_REQUIRE( web.start() );

  auto weburl = web.url();
  weburl.setPathName("/file-1.txt");

  zyppng::TransferSettings set = web.transferSettings();
  zypp::filesystem::TmpFile targetFile;

  auto reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  reqDLFile->transferSettings() = set;
  reqDLFile->setUrl( weburl );
  reqDLFile->addRequestRange(  13, 4 );
  reqDLFile->addRequestRange( 248, 6 );
  reqDLFile->addRequestRange(  76, 9 );
  disp->enqueue( reqDLFile );
  if ( disp->count () ) ev->run();
  auto err = reqDLFile->error();
  BOOST_TEST_REQ_SUCCESS( reqDLFile );

  std::string downloaded = TestTools::readFile ( targetFile.path() );
  BOOST_REQUIRE( !downloaded.empty() );
  BOOST_REQUIRE_EQUAL( std::string_view ( downloaded.data()+13 , 4 ), "SUSE" );
  BOOST_REQUIRE_EQUAL( std::string_view ( downloaded.data()+248, 6 ), "TCP/IP" );
  BOOST_REQUIRE_EQUAL( std::string_view ( downloaded.data()+76 , 9 ), "Slackware" );
}

//Get a simple range from a existing file
BOOST_DATA_TEST_CASE(nwdispatcher_multipart_dl_http, bdata::make( withSSL ), withSSL )
{
  nwdispatcher_multipart_dl_impl<WebServer>(withSSL);
}

BOOST_DATA_TEST_CASE(nwdispatcher_multipart_dl_ftp, bdata::make( withSSL ), withSSL )
{
  nwdispatcher_multipart_dl_impl<FtpServer>(withSSL);
}

// Test that simulates us sending a range request to the server and the server answering with a range header but does not send actual range data
BOOST_DATA_TEST_CASE(nwdispatcher_rangereq_norangeanswer, bdata::make( withSSL ), withSSL )
{
  auto ev = zyppng::EventLoop::create();
  auto disp = std::make_shared<zyppng::NetworkRequestDispatcher>();
  disp->sigQueueFinished().connect( [&ev]( const zyppng::NetworkRequestDispatcher& ){
    ev->quit();
  });

  disp->run();

  WebServer web((zypp::Pathname(TESTS_SRC_DIR)/"data"/"dummywebroot").c_str(), 10001, withSSL );

  web.addRequestHandler("norange", []( WebServer::Request &r ){
    r.rout << "Status: 206\r\n"
              "\r\n"
              "Hello";
  });

  BOOST_REQUIRE( web.start() );

  zyppng::TransferSettings set = web.transferSettings();
  zypp::filesystem::TmpFile targetFile;

  zyppng::Url weburl (web.url());
  weburl.setPathName("/handler/norange");

  zyppng::NetworkRequest::Ptr reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  reqDLFile->transferSettings() = set;
  reqDLFile->setUrl( weburl );
  reqDLFile->addRequestRange(  13, 4 );
  reqDLFile->addRequestRange( 248, 6 );
  reqDLFile->addRequestRange(  76, 9 );

  disp->enqueue( reqDLFile );
  ev->run();

  BOOST_TEST_REQ_ERR( reqDLFile, zyppng::NetworkRequestError::ServerReturnedError );
}

struct RangeData {
  off_t offset;
  std::string payload;
};

auto makeMultiPartHandler ( std::vector<RangeData> &&values )
{
  return [ values = std::move(values) ]( WebServer::Request &r ){
    const char *boundary = "THIS_STRING_SEPARATES";
    r.rout << "Status: 206\r\n"
              "Content-Type: multipart/byteranges; boundary="<<boundary<<"\r\n"
                          "\r\n";

    int fullSize = std::accumulate( values.begin(), values.end(), 0, []( const auto &val1, const auto &val2) { return val1 + val2.payload.length();} );
    for ( const RangeData &val : values ) {
      off_t end = val.offset + val.payload.length() - 1;
      r.rout << "--"<<boundary<<"\r\n"
             << "Content-Type: text/plain\r\n"
             << "Content-Range: bytes "<<val.offset<<"-"<<end<<"/"<<fullSize<<"\r\n"
             << "\r\n"
             << val.payload;
    }
  };
}

//Get a range response with the ranges out of order
BOOST_DATA_TEST_CASE(nwdispatcher_multipart_dl_no_order, bdata::make( withSSL ), withSSL )
{
  auto ev = zyppng::EventLoop::create();
  auto disp = std::make_shared<zyppng::NetworkRequestDispatcher>();
  disp->sigQueueFinished().connect( [&ev]( const zyppng::NetworkRequestDispatcher& ){
    ev->quit();
  });

  disp->run();

  const std::string_view str1 = "Hello";
  const std::string_view str2 = "World";
  const std::string_view str3 = "in Multibyte";

  WebServer web((zypp::Pathname(TESTS_SRC_DIR)/"data"/"dummywebroot").c_str(), 10001, withSSL );
  web.addRequestHandler("mbyte", makeMultiPartHandler( {
    { 10, str2.data() },
    {  0, str1.data() },
    { 25, str3.data() }
  }));

  BOOST_REQUIRE( web.start() );

  auto weburl = web.url();
  weburl.setPathName("/handler/mbyte");

  zyppng::TransferSettings set = web.transferSettings();
  zypp::filesystem::TmpFile targetFile;

  auto reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  reqDLFile->transferSettings() = set;
  reqDLFile->setUrl( weburl );
  reqDLFile->addRequestRange(  0, str1.length() );
  reqDLFile->addRequestRange( 10, str2.length() );
  reqDLFile->addRequestRange( 25, str3.length() );
  disp->enqueue( reqDLFile );
  if ( disp->count () ) ev->run();
  BOOST_TEST_REQ_SUCCESS( reqDLFile );

  std::string downloaded = TestTools::readFile ( targetFile.path() );
  BOOST_REQUIRE( !downloaded.empty() );
  BOOST_REQUIRE_EQUAL( std::string_view ( downloaded.data(), 5 )   , str1 );
  BOOST_REQUIRE_EQUAL( std::string_view ( downloaded.data()+10, 5 ), str2 );
  BOOST_REQUIRE_EQUAL( std::string_view ( downloaded.data()+25 )   , str3 );
}

//Get a range response where the boundary string is part of the data
//Some servers like nginx do not check if the boundary string is inside the data, thus we need
//to make sure the parser is not affected by that
BOOST_DATA_TEST_CASE(nwdispatcher_multipart_dl_weird_data, bdata::make( withSSL ), withSSL )
{
  auto ev = zyppng::EventLoop::create();
  auto disp = std::make_shared<zyppng::NetworkRequestDispatcher>();
  disp->sigQueueFinished().connect( [&ev]( const zyppng::NetworkRequestDispatcher& ){
    ev->quit();
  });

  disp->run();

  const std::string_view str1 = "SUSE Linux";
  const std::string_view str2 = "World--THIS_STRING_SEPARATES A";
  const std::string_view str3 = "Other String";

  WebServer web((zypp::Pathname(TESTS_SRC_DIR)/"data"/"dummywebroot").c_str(), 10001, withSSL );
  web.addRequestHandler("mbyte", makeMultiPartHandler( {
                                    { 0,   str1.data() },
                                    { 25,  str2.data() },
                                    { 70,  str3.data() },
                                  }));

  BOOST_REQUIRE( web.start() );

  auto weburl = web.url();
  weburl.setPathName("/handler/mbyte");

  zyppng::TransferSettings set = web.transferSettings();
  zypp::filesystem::TmpFile targetFile;

  auto reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  reqDLFile->transferSettings() = set;
  reqDLFile->setUrl( weburl );
  reqDLFile->addRequestRange(  0, str1.length() );
  reqDLFile->addRequestRange( 25, str2.length() );
  reqDLFile->addRequestRange( 70, str3.length() );
  disp->enqueue( reqDLFile );
  if ( disp->count () ) ev->run();
  BOOST_TEST_REQ_SUCCESS( reqDLFile );

  std::string downloaded = TestTools::readFile ( targetFile.path() );
  BOOST_REQUIRE( !downloaded.empty() );
  BOOST_REQUIRE_EQUAL( std::string_view ( downloaded.data()   , str1.length() ), str1 );
  BOOST_REQUIRE_EQUAL( std::string_view ( downloaded.data()+25, str2.length() ), str2 );
  BOOST_REQUIRE_EQUAL( std::string_view ( downloaded.data()+70, str3.length() ), str3 );
}

BOOST_DATA_TEST_CASE(nwdispatcher_multipart_dl_overlap, bdata::make( withSSL ), withSSL )
{
  auto ev = zyppng::EventLoop::create();
  auto disp = std::make_shared<zyppng::NetworkRequestDispatcher>();
  disp->sigQueueFinished().connect( [&ev]( const zyppng::NetworkRequestDispatcher& ){
    ev->quit();
  });
  disp->run();

  const char *data = "The SUSE Linux distribution was originally a German translation of Slackware Linux. In mid-1992, Softlanding Linux System (SLS) was founded by Peter MacDonald, and was the first comprehensive distribution to contain elements such as X and TCP/IP. The Slackware distribution (maintained by Patrick Volkerding) was initially based largely on SLS.";
  const std::string_view str1 = std::string_view( data, 50 );
  const std::string_view str2 = std::string_view( data+40 );

  WebServer web((zypp::Pathname(TESTS_SRC_DIR)/"data"/"dummywebroot").c_str(), 10001, withSSL );
  web.addRequestHandler("mbyte", makeMultiPartHandler( {
                                    { 0,   std::string( str1.data(), str1.length() ) },
                                    { 40,  str2.data() }
                                    }));

  BOOST_REQUIRE( web.start() );

  auto weburl = web.url();
  weburl.setPathName("/handler/mbyte");

  zyppng::TransferSettings set = web.transferSettings();
  zypp::filesystem::TmpFile targetFile;

  auto reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  reqDLFile->transferSettings() = set;
  reqDLFile->setUrl( weburl );
  reqDLFile->addRequestRange(  0, str1.length() );
  reqDLFile->addRequestRange( 40, str2.length() );
  disp->enqueue( reqDLFile );
  if ( disp->count () ) ev->run();
  BOOST_TEST_REQ_SUCCESS( reqDLFile );

  std::string downloaded = TestTools::readFile ( targetFile.path() );
  BOOST_REQUIRE( !downloaded.empty() );
  BOOST_REQUIRE_EQUAL( downloaded, std::string(data) );
}

//Get a range response with missing data
BOOST_DATA_TEST_CASE(nwdispatcher_multipart_data_missing, bdata::make( withSSL ), withSSL )
{
  auto ev = zyppng::EventLoop::create();
  auto disp = std::make_shared<zyppng::NetworkRequestDispatcher>();
  disp->sigQueueFinished().connect( [&ev]( const zyppng::NetworkRequestDispatcher& ){
    ev->quit();
  });

  disp->run();

  const std::string_view str1 = "Hello";
  const std::string_view str2 = "World";
  const std::string_view str3 = "in Multibyte";

  WebServer web((zypp::Pathname(TESTS_SRC_DIR)/"data"/"dummywebroot").c_str(), 10001, withSSL );
  web.addRequestHandler("mbyte", makeMultiPartHandler( {
                                    { 10, str2.data() },
                                    {  0, str1.data() },
                                    { 25, str3.data() }
                                  }));

  BOOST_REQUIRE( web.start() );

  auto weburl = web.url();
  weburl.setPathName("/handler/mbyte");

  zyppng::TransferSettings set = web.transferSettings();
  zypp::filesystem::TmpFile targetFile;

  auto reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  reqDLFile->transferSettings() = set;
  reqDLFile->setUrl( weburl );
  reqDLFile->addRequestRange(  0, str1.length() );
  reqDLFile->addRequestRange( 10, str2.length() + 1 );
  reqDLFile->addRequestRange( 25, str3.length() );
  disp->enqueue( reqDLFile );
  if ( disp->count () ) ev->run();
  BOOST_TEST_REQ_ERR( reqDLFile, zyppng::NetworkRequestError::MissingData );
}

// Get many ranges from a file to force the downloader into using multiple batches
// and the server to generate range failure for requesting > 128 ranges ( configured in nginx.conf )
BOOST_DATA_TEST_CASE(nwdispatcher_multipart_many_chunks_dl, bdata::make( withSSL ), withSSL )
{
  auto ev = zyppng::EventLoop::create();
  auto disp = std::make_shared<zyppng::NetworkRequestDispatcher>();
  disp->sigQueueFinished().connect( [&ev]( const zyppng::NetworkRequestDispatcher& ){
    ev->quit();
  });

  disp->run();

  WebServer web((zypp::Pathname(TESTS_SRC_DIR)/"zypp/data/Fetcher/remote-site").c_str(), 10001, withSSL );
  BOOST_REQUIRE( web.start() );

  auto weburl = web.url();
  weburl.setPathName("/file-1.txt");

  auto sourceFile = zypp::Pathname(TESTS_SRC_DIR)/"zypp/data/Fetcher/remote-site/file-1.txt";

  zyppng::TransferSettings set = web.transferSettings();
  zypp::filesystem::TmpFile targetFile;

  zypp::PathInfo pi( sourceFile );

  // request every other chunk from the file
  zypp::ByteCount seekInterval = pi.size () / 512;

  auto reqDLFile = std::make_shared<zyppng::NetworkRequest>( weburl, targetFile.path() );
  reqDLFile->transferSettings() = set;
  reqDLFile->setUrl( weburl );

  auto chunkLen = seekInterval / 2;
  for ( zypp::ByteCount off = 0;( off + chunkLen )< pi.size(); off += seekInterval ) {
    reqDLFile->addRequestRange(  off, chunkLen );
  }

  disp->enqueue( reqDLFile );
  if ( disp->count () ) ev->run();
  auto err = reqDLFile->error();
  BOOST_TEST_REQ_SUCCESS( reqDLFile );

  std::string downloaded = TestTools::readFile ( targetFile.path() );

  BOOST_REQUIRE( !downloaded.empty() );

  std::string sourceData = TestTools::readFile ( sourceFile );

  for ( zypp::ByteCount off = 0;( off + chunkLen )< pi.size(); off += seekInterval ) {
    BOOST_REQUIRE_EQUAL( std::string_view ( sourceData.data()+off , chunkLen ), std::string_view ( downloaded.data()+off , chunkLen ) );
  }
}

