/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_CONTEXT_FWD_INCLUDED
#define ZYPP_NG_CONTEXT_FWD_INCLUDED

#include <zypp-core/zyppng/base/zyppglobal.h>

namespace zyppng {

  struct SyncTag;
  struct AsyncTag;

  ZYPP_FWD_DECL_TYPE_WITH_REFS ( ContextBase );
  ZYPP_FWD_DECL_TEMPL_TYPE_WITH_REFS_ARG1( Context, Tag );

  using AsyncContext = Context<AsyncTag>;
  ZYPP_FWD_DECL_REFS(AsyncContext);

  using SyncContext  = Context<SyncTag>;
  ZYPP_FWD_DECL_REFS(SyncContext);

}
#endif
