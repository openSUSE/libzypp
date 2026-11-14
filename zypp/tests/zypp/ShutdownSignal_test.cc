#include <unistd.h>
#include <glib.h>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <zypp-core/base/userrequestexception.h>
#include <zypp/ZYpp.h>
#include <zypp/zypp_detail/ZYppImpl.h>

using namespace zypp;

// ZYpp::clearShutdownSignal() must reset the shutdown state of zypp_poll
// and must not block if there is nothing to clear.
BOOST_AUTO_TEST_CASE(shutdown_signal_set_clear)
{
  // A blocking clearShutdownSignal() would hang forever: let SIGALRM fail the test instead.
  ::alarm( 30 );

  std::vector<GPollFD> fds;

  // Nothing signaled yet: nothing to clear, polling times out.
  ZYpp::clearShutdownSignal();
  BOOST_CHECK_EQUAL( zypp_detail::zypp_poll( fds, 0 ), 0 );

  // Signaled: polling aborts.
  ZYpp::setShutdownSignal();
  BOOST_CHECK_THROW( zypp_detail::zypp_poll( fds, 0 ), UserRequestException );

  // Cleared: pending signals are drained, polling times out again,
  // and clearing an already empty pipe does not block.
  ZYpp::setShutdownSignal();
  ZYpp::setShutdownSignal();
  ZYpp::clearShutdownSignal();
  BOOST_CHECK_EQUAL( zypp_detail::zypp_poll( fds, 0 ), 0 );
  ZYpp::clearShutdownSignal();

  ::alarm( 0 );
}
