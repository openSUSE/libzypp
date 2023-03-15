/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPPNG_PRIVATE_TASKSTATUS_P_H
#define ZYPPNG_PRIVATE_TASKSTATUS_P_H

#include <zyppng/progressobserver.h>
#include <zyppng/utils/GList>
#include <zyppng/utils/GObjectMemory>
#include <zypp/AutoDispose.h>

#include <zypp-core/zyppng/ui/ProgressObserver>
#include "globals_p.h"

struct ZyppProgressObserverPrivate
  : public zyppng::WrapperPrivateBase<ZyppProgressObserver, zyppng::ProgressObserver, ZyppProgressObserverPrivate>
{

  ZyppProgressObserverPrivate( ZyppProgressObserver *pub ) : WrapperPrivateBase(pub) {};

  zyppng::ProgressObserverRef makeCppType() {
    return zyppng::ProgressObserver::create();
  }

  void assignCppType ( zyppng::ProgressObserverRef &&ptr ) override;

  zyppng::GListContainer<ZyppProgressObserver> _children;
};

struct _ZyppProgressObserver
{
  GObjectClass parent_class;
};

#endif
