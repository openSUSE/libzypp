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

#include <zypp/Target.h>
#include <zypp/ng/media/provide.h>

namespace zypp {
  DEFINE_PTR_TYPE(KeyRing);
  class ZConfig;
}

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS( Context );

  using KeyRing    = zypp::KeyRing;
  using KeyRingRef = zypp::KeyRing_Ptr;

  class ZYPP_API Context {

    ZYPP_ADD_CREATE_FUNC(Context)

  public:
    ZYPP_DECL_PRIVATE_CONSTR(Context);

    using ProvideType = Provide;
    static constexpr auto isAsync = false;

    static ContextRef defaultContext();

    ProvideRef provider() const;
    KeyRingRef keyRing () const;
    zypp::ZConfig &config();
    zypp::ResPool pool();
    zypp::ResPoolProxy poolProxy();
    zypp::sat::Pool satPool();


  private:
    ProvideRef _media;
  };

  template<typename OpType>
  using MaybeAsyncContextRef = std::conditional_t<detail::is_async_op_v<OpType>, ContextRef, ContextRef>;

  template<typename T>
  auto joinPipeline( ContextRef ctx, T &&val ) {
    return std::forward<T>(val);
  }
}

#endif
