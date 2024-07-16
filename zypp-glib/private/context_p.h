/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef CONTEXT_P_H
#define CONTEXT_P_H

#include <zypp-glib/context.h>
#include <zypp-core/zyppng/base/EventDispatcher>
#include <zypp-core/Pathname.h>
#include <zypp-glib/utils/GObjectMemory>
#include <string>

#include "globals_p.h"

// the good ol zypp API
#include <zypp/ng/Context>

struct ZyppContextPrivate
  : public zypp::glib::WrapperPrivateBase<ZyppContext, zyppng::SyncContext, ZyppContextPrivate>
{
  ZyppContextPrivate( ZyppContext *pub ) : WrapperPrivateBase(pub) {};

  void initializeCpp();
  zyppng::SyncContextRef &cppType();

  struct ConstructionProps {
    zypp::Pathname     _sysRoot = "/";
    zyppng::SyncContextRef _cppObj;
  };
  std::optional<ConstructionProps> _constructProps = ConstructionProps();

  std::string version = LIBZYPP_VERSION_STRING;
  zyppng::SyncContextRef _context;
};

/*
 * This is the GObject derived type, it is simply used to host our c++ data object and is
 * passed as a opaque pointer to the user code.
 * This is always created with malloc, so no con or destructors are executed here!
 */
struct _ZyppContext
{
  GObjectClass            parent_class;
};

/**
 * zypp_context_get_cpp: (skip)
 */
zyppng::SyncContextRef zypp_context_get_cpp( ZyppContext *self );

#endif // CONTEXT_P_H
