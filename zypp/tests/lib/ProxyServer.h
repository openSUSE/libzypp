
#ifndef ZYPP_TEST_ProxyServer_H
#define ZYPP_TEST_ProxyServer_H

#include <zypp/Url.h>
#include <zypp/Pathname.h>
#include <zypp/base/PtrTypes.h>
#include <zypp-curl/TransferSettings>

#include <functional>

/**
 *
 * Starts a proxy server to simulate web access via a proxy,
 * use together with WebServer to do something useful
 *
 * \code
 * #include "ProxyServer.h"
 *
 * BOOST_AUTO_TEST_CASE(Foo)
 * {
 *  WebServer web( testRoot.c_str(), 10001 );
 *
 *  ProxyServer proxy( 10002 );
 *  proxy.setAuthEnabled ( true );
 *
 *  BOOST_REQUIRE( web.start() );
 *  BOOST_REQUIRE( proxy.start() );
 *
 *  zypp::media::MediaManager   mm;
 *  zypp::media::MediaAccessId  id;
 *
 *  zypp::Url mediaUrl = web.url();
 *
 *  mediaUrl.setQueryParam( "proxy", proxy.url().asCompleteString() );
 *
 *  // if  auth is enabled
 *  mediaUrl.setQueryParam( "proxyuser", "user1");
 *  mediaUrl.setQueryParam( "proxypass", "pass1");
 *
 *  // do something with the url
 *
 *
 *  proxy.stop();
 *  web.stop();
 *
 * \endcode
 */
class ProxyServer
{
 public:

  /**
   * creates a http proxy server listening on \a port
   */
   ProxyServer(unsigned int port=10002);
   ~ProxyServer();

  /**
   * Starts the webserver worker thread
   * Waits up to 10 seconds and returns whether the port is now active.
   */
  bool start();

  /**
   * Stops the worker thread
   */
  void stop();

  /**
   * returns the port we are listening to
   */
  int port() const;

  /**
   * returns the proxy URL
   */
  zypp::Url url() const;

  /**
   * shows the log of last run
   */
  std::string log() const;

  /**
   * Checks if the server was stopped
   */
  bool isStopped() const;


  /**
   * Enable proxy authentication, user=test, pass=test
   */
  void setAuthEnabled( bool set = true );

  class Impl;
private:
  /** Pointer to implementation */
  zypp::RW_pointer<Impl> _pimpl;
};

#endif
