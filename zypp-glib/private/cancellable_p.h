/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include <zypp-core/zyppng/async/asyncop.h>
#include <zypp-core/zyppng/pipelines/expected.h>
#include <zypp-core/zyppng/base/socketnotifier.h>
#include <zypp-glib/utils/GLibMemory>
#include <zypp-glib/utils/GObjectMemory>
#include <zypp-glib/utils/GioMemory>

namespace zypp::glib {

  template <typename T>
  class CancellableOp : public zyppng::AsyncOp<zyppng::expected<T>>
  {
    // AbstractEventSource interface
  public:

    CancellableOp( zyppng::AsyncOpRef<zyppng::expected<T>> op, GCancellableRef cancellable )
      : _op( std::move(op) )
      , _cancellable( std::move(cancellable) )
    {
      if ( _cancellable ) {
        _cancellableWatcher = zyppng::SocketNotifier::create( g_cancellable_get_fd( _cancellable.get()), zyppng::SocketNotifier::Read );
        _cancellableWatcher->connect( &zyppng::SocketNotifier::sigActivated, this, &CancellableOp::cancel );
      }
      _op->onReady( [this]( zyppng::expected<T> result ) { pipelineReady(std::move(result)); });
    }


  private:

    void cancel () {
      _op.reset(); // cancel by calling the destructor
      pipelineReady ( ZYPP_EXCPT_PTR( zypp::Exception("Cancelled by User.") ) );
    }

    void pipelineReady( zyppng::expected<T> &&result ) {
      if ( _cancellableWatcher ) {
        _cancellableWatcher->setEnabled ( false );
        _cancellableWatcher.reset();
        g_cancellable_release_fd ( _cancellable.get() );
      }
      setReady( std::move(result) );
    }

    zyppng::AsyncOpRef<zyppng::expected<T>> _op;
    zyppng::SocketNotifierRef _cancellableWatcher;
    GCancellableRef _cancellable;

    int _myFd = -1;
  };
}
