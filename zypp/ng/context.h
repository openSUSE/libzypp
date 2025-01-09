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
#include <zypp/ng/workflows/logichelpers.h>
#include <zypp/ng/ContextBase>
#include <zypp-core/zyppng/base/EventLoop>
#include <zypp-core/zyppng/base/EventDispatcher>
#include <zypp-core/zyppng/pipelines/algorithm.h>

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
  class Context : public ContextBase, public MaybeAsyncMixin< std::is_same_v<Tag, AsyncTag> >,  private detail::ContextData<Tag> {

    ZYPP_ADD_CREATE_FUNC(Context)

  public:
    ZYPP_DECL_PRIVATE_CONSTR(Context)
    { }

    ZYPP_ENABLE_MAYBE_ASYNC_MIXIN( (std::is_same_v<Tag, AsyncTag>) );
    using ProvideType     = std::conditional_t< std::is_same_v<Tag, AsyncTag>, Provide, MediaSyncFacade >;
    using SyncOrAsyncTag  = Tag;

    auto provider() {
      return this->_provider;
    }

    /**
     * Tries to lock a resource \a ident with specified \a mode , if the resource is locked already,
     * a delayed lock request is enqueued in a list of waiters for said lock. A \a timeout is required.
     */
    MaybeAsyncRef<expected<ResourceLockRef>> lockResourceWait( std::string ident, uint timeout, ResourceLockRef::Mode mode = ResourceLockRef::Shared )
    {
      if constexpr ( std::is_same_v<Tag, AsyncTag> ) {
        auto res = this->lockResource ( ident, mode );
        if ( res ) {
          return makeReadyResult ( std::move(res) );
        } else {
          // any other exception than "already locked" is directly forwarded
          if ( !containsException<ResourceAlreadyLockedException>(res.error()) ) {
            return makeReadyResult ( std::move(res) );
          }

          const auto &lockEntry = this->_resourceLocks.find(ident);
          if ( lockEntry == this->_resourceLocks.end() ) {
            ERR << "Could not find resource entry, this is a bug, trying to lock directly again!" << std::endl;
            return makeReadyResult(this->lockResource ( ident, mode ));
          }

          AsyncResourceLockReqRef promise = std::make_shared<AsyncResourceLockReq>( ident, mode, timeout );
          lockEntry->second->_lockQueue.push_back(promise);
          promise->connect( &AsyncResourceLockReq::sigTimeout, *this, &Context::onResourceLockTimeout );
          return promise;
        }
      } else {
        // no waiting in sync context
        return this->lockResource ( ident, mode );
      }
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


  private:
    void onResourceLockTimeout( AsyncResourceLockReq &req ) {
      // just deque the lock, finishing the pipeline is done directly by the request itself
      const auto &lockEntry = this->_resourceLocks.find( req.ident () );
      if ( lockEntry == this->_resourceLocks.end() ) {
        ERR << "Could not find lock ident after receiving a timeout signal from it, this is a BUG." << std::endl;
        return;
      }

      detail::ResourceLockData &lockData = *lockEntry->second.get();
      auto i = std::find_if( lockData._lockQueue.begin (), lockData._lockQueue.end(), [&req]( auto &weakLockOp ){
        auto lockOp = weakLockOp.lock();
        return ( lockOp && (lockOp.get() == &req) );
      });

      if ( i == lockData._lockQueue.end() ) {
        ERR << "Could not find lock request after receiving a timeout signal from it, this is a BUG." << std::endl;
        return;
      }

       lockData._lockQueue.erase(i);
    }

    void dequeueWaitingLocks( detail::ResourceLockData &resLckData ) override {
      if constexpr ( std::is_same_v<Tag, AsyncTag> ) {

        // callable to be executed when event loop is idle
        const auto &makeResultCallable = []( std::weak_ptr<AsyncResourceLockReq> asyncOp, expected<ResourceLockRef> &&result ) {
          // the STL does copy the lambda around inside the std::function implementation instead of moving it. So we need to make it copy'able
          // which means storing the ResourceLock in a shared_ptr.
          return [ asyncOp, res = std::make_shared<expected<ResourceLockRef>>(std::move(result)) ] () mutable {
            auto promise = asyncOp.lock();
            if ( promise )
              promise->setFinished ( std::move(*res.get()) );
            return false; // stop calling after the first time
          };
        };

        // currently we got no lock, we now will give locks to the next waiters
        bool canCont = true;
        bool isFirst = true;
        while ( canCont && !resLckData._lockQueue.empty() ) {
          auto front = resLckData._lockQueue.front().lock();
          if ( !front ) {
            resLckData._lockQueue.pop_front ();
            continue;
          }
          if ( isFirst ) {
            isFirst = false;
            //  first iteration, we can simply give the lock no matter if shared or not
            resLckData._mode = front->mode();
            // we can continue if the mode is shared, the next iteration will make sure the next one in the queue
            // has shared mode too
            canCont = ( resLckData._mode == ResourceLockRef::Shared );
            EventDispatcher::invokeOnIdle( makeResultCallable( front, make_expected_success( ResourceLockRef( detail::ResourceLockData_Ptr(&resLckData) ) ) ) );
            resLckData._lockQueue.pop_front();
          } else if ( front->mode() == ResourceLockRef::Shared ) {
            // no need to further check the lock's mode, if we end up here the first iteration already
            // checked that it is a shared lock and set canCont accordingly
            resLckData._lockQueue.pop_front();
            EventDispatcher::invokeOnIdle( makeResultCallable( front, make_expected_success( ResourceLockRef( detail::ResourceLockData_Ptr(&resLckData) ) ) ) );
          } else {
            // front does not have a shared lock, we need to stop here
            break;
          }
        }
      } else {
        // in non async mode lock waiting is not supported
        // while this code should never be executed i'll add it for completeness
        while ( !resLckData._lockQueue.empty() ) {
          auto front = resLckData._lockQueue.front().lock();
          if ( !front ) {
            resLckData._lockQueue.pop_front ();
            continue;
          }
          front->setFinished( expected<ResourceLockRef>::error( ZYPP_EXCPT_PTR( zypp::Exception("Lock waits not supported in sync mode")) ) );
          resLckData._lockQueue.pop_front ();
        }
      }
    }
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
