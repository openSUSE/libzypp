/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_PRIVATE_CONTEXT_P_H
#define ZYPP_NG_PRIVATE_CONTEXT_P_H

#include <zypp-core/zyppng/base/private/base_p.h>
#include <zypp-core/zyppng/ui/private/userinterface_p.h>
#include <zypp/ng/context.h>
#include <zypp/ZYpp.h>

namespace zyppng
{
  ZYPP_FWD_DECL_TYPE_WITH_REFS ( EventDispatcher );

  class ContextPrivate : public UserInterfacePrivate
  {
  public:
    ContextPrivate ( Context &b ) : UserInterfacePrivate(b){}
    EventDispatcherRef _eventDispatcher;
    ProvideRef _provider;
    zypp::ZYpp::Ptr _zyppPtr;
  };

}


#endif
