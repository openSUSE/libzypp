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


  /*!
   * Returns the default sync zypp context.
   * This is only a workaround to support the Legacy APIs.
   * For new style APIs the Context should always explicitely be passed down. So use it
   * only in top level external classes where we can not touch the API.
   */
  SyncContextRef SyncContext::defaultContext()
  {
    static SyncContextRef def = SyncContext::create();
    return def;
  }

  MediaSyncFacadeRef SyncContext::provider() const
  {
    return _media;
  }

  KeyRingRef SyncContext::keyRing() const
  {
    return zypp::getZYpp()->keyRing();
  }

  zypp::ZConfig &SyncContext::config()
  {
    return zypp::ZConfig::instance();
  }

  zypp::ResPool SyncContext::pool()
  {
    return zypp::ResPool::instance();
  }

  zypp::ResPoolProxy SyncContext::poolProxy()
  {
    return zypp::ResPool::instance().proxy();
  }

  zypp::sat::Pool SyncContext::satPool()
  {
    return zypp::sat::Pool::instance();
  }
}
