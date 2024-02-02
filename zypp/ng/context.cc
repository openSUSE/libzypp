/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/context_p.h"
#include <zypp/ZYppFactory.h>
#include <zypp-core/zyppng/base/private/threaddata_p.h>
#include <zypp-core/zyppng/base/EventLoop>
#include <zypp-media/ng/Provide>

namespace zyppng {

  ZYPP_IMPL_PRIVATE( Context )

  ZYPP_IMPL_PRIVATE_CONSTR( Context )
    : UserInterface( *new ContextPrivate( *this ) )
  {
    Z_D();
    d->_zyppPtr = zypp::getZYpp();
    d->_eventDispatcher = ThreadData::current().ensureDispatcher();

    d->_provider = Provide::create( d->_providerDir );

    // @TODO should start be implicit as soon as something is enqueued?
    d->_provider->start();
  }

  ProvideRef Context::provider() const
  {
    Z_D();
    return d->_provider;
  }

  KeyRingRef Context::keyRing() const
  {
    return d_func()->_zyppPtr->keyRing();
  }

  zypp::ZConfig &Context::config()
  {
    return zypp::ZConfig::instance();
  }

  void Context::executeImpl(AsyncOpBaseRef op)
  {
    auto loop = EventLoop::create();
    op->sigReady().connect([&](){
      loop->quit();
    });
    loop->run();
    return;
  }
}
