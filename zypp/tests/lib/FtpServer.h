
#ifndef ZYPP_TEST_FTPSERVER_H
#define ZYPP_TEST_FTPSERVER_H

#include <zypp-core/Url.h>
#include <zypp-core/Pathname.h>
#include <zypp-core/base/PtrTypes.h>
#include <zypp-curl/TransferSettings>

#include <functional>

/**
 *
 * Starts a ftp server to simulate remote transfers in
 * testcases
 *
 * \code
 * #include "FtpServer.h"
 *
 * BOOST_AUTO_TEST_CASE(Foo)
 * {
 *
 *      FtpServer srv((Pathname(TESTS_SRC_DIR) + "/datadir").c_str() );
 *      srv.start();
 *
 *     MediaSetAccess media( Url("ftp://localhost:9099"), "/" );
 *
 *     // do something with the url
 *
 *
 *     srv.stop();
 *
 * \endcode
 */
class FtpServer
{
 public:

  /**
   * creates a web server on \ref root and \port
   */
   FtpServer(const zypp::Pathname &root, unsigned int port=10001, bool useSSL = false );
   ~FtpServer();
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
   * returns the base url where the webserver is listening
   */
  zypp::Url url() const;

  /**
   * generates required transfer settings
   */
  zypp::media::TransferSettings transferSettings () const;

  /**
   * shows the log of last run
   */
  std::string log() const;

  /**
   * Checks if the server was stopped
   */
  bool isStopped() const;

  class Impl;
private:
  /** Pointer to implementation */
  zypp::RW_pointer<Impl> _pimpl;
};

#endif
