/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include <zypp-core/zyppng/base/abstracteventsource.h>

namespace zypp::glib {

  class CancellableOp : public zyppng::AbstractEventSource
  {
    // AbstractEventSource interface
  public:
    void onFdReady(int fd, int events) override;
    void onSignal(int signal) override;
  };
}
