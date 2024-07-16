/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_SYNCCONTEXT_INCLUDED
#define ZYPP_NG_SYNCCONTEXT_INCLUDED

#include <zypp/ng/context_fwd.h>
#include <zypp-core/fs/TmpPath.h>
#include <zypp-core/zyppng/base/private/threaddata_p.h>
#include <zypp/ZConfig.h>
#include <zypp/ng/workflows/mediafacade.h>
#include <zypp/ng/ContextBase>
#include <zypp-core/zyppng/base/EventLoop>

namespace zyppng {

  struct SyncTag{};
  struct AsyncTag{};

  namespace detail {

    template <typename Tag>
    struct ContextData;

    template <>
    struct ContextData<SyncTag>
    {
      ContextData()
      : _provider( MediaSyncFacade::create()) {}

      MediaSyncFacadeRef _provider;
    };

    template <>
    struct ContextData<AsyncTag>
    {
      ContextData()
        : _eventDispatcher(ThreadData::current().ensureDispatcher())
        , _provider( Provide::create() ){
        // @TODO should start be implicit as soon as something is enqueued?
        _provider->start();
      }
      zyppng::EventDispatcherRef _eventDispatcher;
      ProvideRef _provider;
    };
  }

  template <typename Tag>
  class Context : public ContextBase, private detail::ContextData<Tag> {

    ZYPP_ADD_CREATE_FUNC(Context)

  public:
    ZYPP_DECL_PRIVATE_CONSTR(Context)
    { }

    using ProvideType     = std::conditional_t< std::is_same_v<Tag, AsyncTag>, Provide, MediaSyncFacade >;
    using SyncOrAsyncTag  = Tag;

    auto provider() {
      return this->_provider;
    }

    template <typename AsyncRes, typename CtxTag = Tag >
    std::enable_if_t< std::is_same_v<CtxTag, AsyncTag>> execute ( AsyncOpRef<AsyncRes> op ) {
      if ( op->isReady () )
        return;

      auto loop = EventLoop::create( this->_eventDispatcher );
      op->sigReady().connect([&](){
        loop->quit();
      });
      loop->run();
      return;
    }

    using ContextBase::legacyInit;
    using ContextBase::changeToTarget;
  };

  template<typename OpType>
  using MaybeAsyncContextRef = std::conditional_t<detail::is_async_op_v<OpType>, AsyncContextRef, SyncContextRef>;

  template<typename T>
  auto joinPipeline( SyncContextRef ctx, T &&val ) {
    return std::forward<T>(val);
  }

  template<typename T>
  auto joinPipeline( AsyncContextRef ctx, AsyncOpRef<T> res ) {
    if constexpr ( detail::is_async_op_v<decltype(res)> ) {
      ctx->execute(res);
      return res->get();
    } else {
      return res;
    }
  }
}

#endif
