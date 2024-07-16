/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_PRIVATE_TASKSTATUS_P_H
#define ZYPP_GLIB_PRIVATE_TASKSTATUS_P_H

#include <zypp-glib/progressobserver.h>
#include <zypp-glib/utils/GList>
#include <zypp-glib/utils/GObjectMemory>
#include <zypp/AutoDispose.h>

#include <zypp-core/zyppng/ui/ProgressObserver>
#include "globals_p.h"

struct ZyppProgressObserverPrivate
  : public zypp::glib::WrapperPrivateBase<ZyppProgressObserver, zyppng::ProgressObserver, ZyppProgressObserverPrivate>
{

  ZyppProgressObserverPrivate( ZyppProgressObserver *pub ) : WrapperPrivateBase(pub) {};

  zyppng::ProgressObserverRef &cppType() {
    return _cppObj;
  }

  void initializeCpp();;

  struct ConstrProps {
    zyppng::ProgressObserverRef _cppObj;
  };
  std::optional<ConstrProps> _constrProps = ConstrProps();


  zyppng::ProgressObserverRef _cppObj;
  zypp::glib::GListContainer<ZyppProgressObserver> _children;
};

struct _ZyppProgressObserver
{
  GObjectClass parent_class;
};

#endif
