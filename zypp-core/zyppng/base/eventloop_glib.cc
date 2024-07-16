#include "private/eventloop_glib_p.h"
#include <zypp-core/zyppng/base/EventDispatcher>

namespace zyppng {

  EventLoopPrivate::EventLoopPrivate( EventLoop &p ) : BasePrivate( p )
  { }

  ZYPP_IMPL_PRIVATE(EventLoop)

  EventLoop::EventLoop( EventDispatcherRef disp )
    : Base ( * new EventLoopPrivate(*this) )
  {
    Z_D();
    d->_dispatcher = std::move(disp);
    d->_loop       = g_main_loop_new( reinterpret_cast<GMainContext*>(d->_dispatcher->nativeDispatcherHandle()), false );
  }


  EventLoop::EventLoop( GMainContext *ctx )
    : EventLoop( ThreadData::current().ensureDispatcher( ctx ) )
  {}

  EventLoop::~EventLoop()
  {
    g_main_loop_unref( d_func()->_loop );
  }

  EventLoop::Ptr EventLoop::create( GMainContext *ctx )
  {
    return Ptr( new EventLoop( ctx ) );
  }

  EventLoop::Ptr EventLoop::create( EventDispatcherRef dispatcher  )
  {
    return Ptr( new EventLoop( std::move(dispatcher) ) );
  }


  void EventLoop::run()
  {
    Z_D();
    g_main_context_push_thread_default( reinterpret_cast<GMainContext*>(d->_dispatcher->nativeDispatcherHandle()) );
    zypp_defer {
      d->_dispatcher->clearUnrefLaterList();
      g_main_context_pop_thread_default( reinterpret_cast<GMainContext*>(d->_dispatcher->nativeDispatcherHandle()) );
    };
    g_main_loop_run( d->_loop );
  }

  void EventLoop::quit()
  {
    g_main_loop_quit( d_func()->_loop );
  }

  std::shared_ptr<EventDispatcher> EventLoop::eventDispatcher() const
  {
    return d_func()->_dispatcher;
  }
}
