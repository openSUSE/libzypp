/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_CONTEXTFACADE_INCLUDED
#define ZYPP_NG_CONTEXTFACADE_INCLUDED

#include "zypp/ng/context.h"
#include <zypp/ng/workflows/mediafacade.h>

namespace zypp {
    DEFINE_PTR_TYPE(KeyRing);
}

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS( SyncContext );

  using KeyRing    = zypp::KeyRing;
  using KeyRingRef = zypp::KeyRing_Ptr;

  class SyncContext {

    ZYPP_ADD_CREATE_FUNC(SyncContext)

  public:
    ZYPP_DECL_PRIVATE_CONSTR(SyncContext);

    using ProvideType = MediaSyncFacade;

    MediaSyncFacadeRef provider() const;
    KeyRingRef keyRing () const;

  private:
    MediaSyncFacadeRef _media;
  };

  template<typename OpType>
  using MaybeAsyncContextRef = std::conditional_t<detail::is_async_op_v<OpType>, ContextRef, SyncContextRef>;


}



#endif
