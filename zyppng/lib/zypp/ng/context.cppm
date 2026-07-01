/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
module;
#include <filesystem>
#include <zypp-core/ng/async/awaitable.h>
#include <zypp-core/ng/ui/UserInterface>
#include <zypp-core/ng/base/eventloop.h>
#include <zypp-core/ng/base/zyppglobal.h>
#include <zypp-media/ng/providefwd.h>

export module zyppng:context;

import :contextconfig;

export namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS( Context );

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

    ZYPP_DECL_PRIVATE_CONSTR_ARGS(Context, std::filesystem::path root);

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

    /** Access the configuration for this context. */
    ContextConfig & config();
    const ContextConfig & config() const;

  };

  template<typename T>
  auto joinPipeline( ContextRef ctx, Task<T> res ) {
    ctx->execute( std::move(res) );
    return res->get();
  }

}