/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include <zypp/ng/context_fwd.h>
#include <zypp-core/zyppng/ui/progressobserver.h>

namespace zyppng {

  class LegacyReportReceiverBase
  {
  public:
    virtual ContextBaseRef context() = 0;
    virtual ProgressObserverRef masterProgress() = 0;
  };

}
