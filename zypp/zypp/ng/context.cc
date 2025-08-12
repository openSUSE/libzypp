/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "context.h"
#include <zypp/ZYppFactory.h>

namespace zyppng
{
  ZYPP_IMPL_PRIVATE_CONSTR(Context) : _media( Provide::create() )
  {

  }


  /*!
   * Returns the default sync zypp context.
   * This is only a workaround to support the Legacy APIs.
   * For new style APIs the Context should always explicitely be passed down. So use it
   * only in top level external classes where we can not touch the API.
   */
  ContextRef Context::defaultContext()
  {
    static ContextRef def = Context::create();
    return def;
  }

  ProvideRef Context::provider() const
  {
    return _media;
  }

  KeyRingRef Context::keyRing() const
  {
    return zypp::getZYpp()->keyRing();
  }

  zypp::ZConfig &Context::config()
  {
    return zypp::ZConfig::instance();
  }

  zypp::ResPool Context::pool()
  {
    return zypp::ResPool::instance();
  }

  zypp::ResPoolProxy Context::poolProxy()
  {
    return zypp::ResPool::instance().proxy();
  }

  zypp::sat::Pool Context::satPool()
  {
    return zypp::sat::Pool::instance();
  }
}
