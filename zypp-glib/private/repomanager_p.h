/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_GLIB_PRIVATE_REPOMANAGER_P_H
#define ZYPP_GLIB_PRIVATE_REPOMANAGER_P_H

#include <zypp-glib/repomanager.h>
#include <zypp-glib/private/context_p.h>
#include <zypp/ng/repomanager.h>

#include "globals_p.h"

struct ZyppRepoManagerPrivate
    : public zypp::glib::WrapperPrivateBase<ZyppRepoManager, zyppng::AsyncRepoManager, ZyppRepoManagerPrivate>
{
  ZyppRepoManagerPrivate( ZyppRepoManager *pub ) : WrapperPrivateBase(pub) {};

  void initializeCpp();
  zyppng::SyncRepoManagerRef &cppType()  {
    return _cppObj;
  }

  struct ConstructData {
    zyppng::SyncRepoManagerRef _cppObj;
    zypp::glib::ZyppContextRef _ctx;
  };
  std::optional<ConstructData> _constrProps = ConstructData();

  zyppng::SyncRepoManagerRef _cppObj;
};

struct _ZyppRepoManager
{
  GObjectClass            parent_class;
};
#endif // ZYPP_GLIB_PRIVATE_REPOMANAGER_P_H
