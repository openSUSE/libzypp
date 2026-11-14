/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/tests/zypp/ShutdownSignal_test.cc
 *
 * The shutdown pipe drives both backends: the legacy \ref zypp_detail::zypp_poll
 * throws while it is readable, and the Curl2 executor watches the same fd. These
 * tests pin the fd level contract the Curl2 change relies on:
 *  - setShutdownSignal() makes it readable and stays that way until cleared,
 *  - clearShutdownSignal() drains it (previously it read the write end and was a
 *    no-op).
 */
#include <boost/test/unit_test.hpp>

#include <glib.h>

#include <zypp/ZYpp.h>
#include <zypp-core/base/UserRequestException>
#include <zypp/zypp_detail/ZYppImpl.h>

#include <vector>

using namespace zypp;

namespace {

  // zypp_poll() throws UserRequestException(ABORT) exactly while the shutdown pipe is
  // readable; it does not drain the pipe.
  bool shutdownPending()
  {
    std::vector<GPollFD> fds;
    try {
      zypp_detail::zypp_poll( fds, 0 );
    }
    catch ( const UserRequestException & ) {
      return true;
    }
    return false;
  }

} // anonymous namespace

BOOST_AUTO_TEST_CASE(set_marks_pending_and_clear_drains)
{
  // Start clean (this also lazily creates the pipe).
  ZYpp::clearShutdownSignal();
  BOOST_CHECK( !shutdownPending() );

  ZYpp::setShutdownSignal();
  BOOST_CHECK( shutdownPending() );

  // clear() must drain the read end. On the previous code it read the wrong end of the
  // pipe and never cleared anything.
  ZYpp::clearShutdownSignal();
  BOOST_CHECK( !shutdownPending() );

  // A later request latches again.
  ZYpp::setShutdownSignal();
  BOOST_CHECK( shutdownPending() );

  ZYpp::clearShutdownSignal();
  BOOST_CHECK( !shutdownPending() );
}

BOOST_AUTO_TEST_CASE(a_single_clear_drains_repeated_requests)
{
  ZYpp::clearShutdownSignal();

  ZYpp::setShutdownSignal();
  ZYpp::setShutdownSignal();
  ZYpp::setShutdownSignal();
  BOOST_CHECK( shutdownPending() );

  ZYpp::clearShutdownSignal();
  BOOST_CHECK( !shutdownPending() );
}
