#ifndef ZYPP_NG_UI_PRIVATE_USERINTERFACE_P_H
#define ZYPP_NG_UI_PRIVATE_USERINTERFACE_P_H

#include <zypp-core/ng/ui/userinterface.h>
#include <zypp-core/ng/base/Signals>
#include <zypp-core/ng/base/private/base_p.h>

namespace zyppng {

  class UserInterfacePrivate : public BasePrivate
  {
    ZYPP_DECLARE_PUBLIC(UserInterface)
  public:
    UserInterfacePrivate( UserInterface &b ) : BasePrivate(b) {}

    Signal<void( UserRequestRef event)> _sigEvent;
  };

}


#endif
