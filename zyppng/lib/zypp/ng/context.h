/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_CONTEXT_INCLUDED
#define ZYPP_NG_CONTEXT_INCLUDED

#include <zypp-core/ng/async/awaitable.h>
#include <zypp-core/ng/ui/UserInterface>
#include <zypp-core/ng/base/eventloop.h>
#include <zypp/ResPool.h>

namespace zypp {
  DEFINE_PTR_TYPE(KeyRing);
  class ZConfig;
}

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS( Context );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( ProgressObserver );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( Provide );

  using KeyRing    = zypp::KeyRing;
  using KeyRingRef = zypp::KeyRing_Ptr;

  class ContextPrivate;


  /*!
   * The Context class is the central object of libzypp, carrying all state that is
   * required to manage the system.
   */
  class Context : public UserInterface
  {
    ZYPP_DECLARE_PRIVATE(Context)
    ZYPP_ADD_CREATE_FUNC(Context)

  public:

    using ProvideType = Provide;
    static constexpr auto isAsync = true;

    ZYPP_DECL_PRIVATE_CONSTR(Context);

    template <typename AsyncRes>
    void execute ( Task<AsyncRes> op ) {
      if ( op->isReady () )
        return;

      auto loop = EventLoop::create();
      op.registerNotifyCallback([&](){
        loop->quit();
      });
      loop->run();
      return;
    }

    ProvideRef provider() const;
    KeyRingRef keyRing () const;
    zypp::ZConfig &config();

    /**
     * Access to the resolvable pool.
     */
    zypp::ResPool pool();

    /** Pool of ui::Selectable.
     * Based on the ResPool, ui::Selectable groups ResObjetcs of
     * same kind and name.
    */
    zypp::ResPoolProxy poolProxy();

    /**
     * Access to the sat pool.
     */
    zypp::sat::Pool satPool();

  };

  template<typename T>
  auto joinPipeline( ContextRef ctx, Task<T> res ) {
    ctx->execute( std::move(res) );
    return res->get();
  }

}


#endif
