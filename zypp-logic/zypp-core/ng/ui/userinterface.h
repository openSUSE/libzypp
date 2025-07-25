/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_CORE_ZYPPNG_USERINTERFACE_H
#define ZYPP_CORE_ZYPPNG_USERINTERFACE_H

#include <zypp-core/ng/base/Base>
#include <zypp-core/ng/base/Signals>

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS( UserInterface );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( UserRequest );

  class UserInterfacePrivate;

  /*!
   * Generic code that allows libzypp to send messages and request
   * directly to the user code.
   *
   * Every time a new request is sent the \ref UserInterface::sigEvent signal
   * is emitted, which deliveres the specific request. Depending on the
   * type of request the frontend code can return data to libzypp, for example
   * if a user wants to trust a key, or a simple password request.
   *
   */
  class UserInterface : public Base
  {
    ZYPP_DECLARE_PRIVATE(UserInterface)
    ZYPP_ADD_CREATE_FUNC(UserInterface)

  public:
    ZYPP_DECL_PRIVATE_CONSTR(UserInterface);
    void sendUserRequest( const UserRequestRef& event );
    SignalProxy<void( UserRequestRef event)> sigEvent();

  protected:
    UserInterface( UserInterfacePrivate &d );
  };

}

#endif
