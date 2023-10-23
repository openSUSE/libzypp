/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "contextfacade.h"
#include <zypp/ZYppFactory.h>

namespace zyppng
{
  ZYPP_IMPL_PRIVATE_CONSTR(SyncContext) : _media( MediaSyncFacade::create() )
  {

  }

  MediaSyncFacadeRef SyncContext::provider() const
  {
    return _media;
  }

  KeyRingRef SyncContext::keyRing() const
  {
    return zypp::getZYpp()->keyRing();
  }
}
