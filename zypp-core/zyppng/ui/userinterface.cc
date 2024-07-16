/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "private/userinterface_p.h"
#include <zypp-core/zyppng/ui/UserRequest>

namespace zyppng {

  ZYPP_IMPL_PRIVATE(UserInterface)

  ZYPP_IMPL_PRIVATE_CONSTR( UserInterface ) : Base ( *( new UserInterfacePrivate(*this) ) ) { }

  UserInterface::UserInterface() : Base ( *( new UserInterfacePrivate(*this) ) ) { }

  UserInterface::UserInterface( UserInterfacePrivate &d ) : Base(d)
  { }

  void UserInterface::sendUserRequest(const UserRequestRef& event)
  {
    Z_D();
    d->_sigEvent.emit( event );
  }

  SignalProxy<void (UserRequestRef)> UserInterface::sigEvent()
  {
    return d_func()->_sigEvent;
  }

}
