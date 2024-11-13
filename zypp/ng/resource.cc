/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "resource.h"
#include <zypp-core/zyppng/base/timer.h>
#include <zypp/ng/context.h>
namespace zyppng {

  namespace detail {
    IMPL_PTR_TYPE( ResourceLockData );

    void ResourceLockData::unref_to( unsigned int n ) const
    {
      if ( n > 1 )
        return;

      auto ctx = _zyppContext.lock ();
      if ( !ctx )
        return;
      ctx->lockUnref ( _resourceIdent );
    }

  }

  ResourceAlreadyLockedException::ResourceAlreadyLockedException( std::string ident )
    : Exception( "Resource Already Locked Exception" )
    , _ident( std::move(ident) )
  { }

  const std::string &ResourceAlreadyLockedException::ident() const
  {
    return _ident;
  }

  std::ostream &ResourceAlreadyLockedException::dumpOn(std::ostream &str) const
  {
    str << "[" << _ident << "] ";
    return Exception::dumpOn( str );
  }

  ResourceLockTimeoutException::ResourceLockTimeoutException(std::string ident)
    : Exception( "Resource Lock Timeout Exception" )
    , _ident( std::move(ident) )
  { }

  const std::string &ResourceLockTimeoutException::ident() const
  {
    return _ident;
  }

  std::ostream &ResourceLockTimeoutException::dumpOn(std::ostream &str) const
  {
    str << "[" << _ident << "] ";
    return Exception::dumpOn( str );
  }

  AsyncResourceLockReq::AsyncResourceLockReq( std::string ident, ResourceLock::Mode m, uint timeout )
    : _ident( std::move(ident) )
    , _mode( m )
    , _timeout( Timer::create() )
  {
    _timeout->setSingleShot (true);
    _timeout->connect( &Timer::sigExpired, *this, &AsyncResourceLockReq::onTimeoutExceeded );
    _timeout->start( timeout );
  }

  void AsyncResourceLockReq::setFinished(expected<ResourceLock> &&lock)
  {
    _timeout->stop();
    setReady ( std::move(lock) );
  }

  const std::string &AsyncResourceLockReq::ident() const
  {
    return _ident;
  }

  ResourceLock::Mode AsyncResourceLockReq::mode() const
  {
    return _mode;
  }

  SignalProxy<void (AsyncResourceLockReq &)> AsyncResourceLockReq::sigTimeout()
  {
    return _sigTimeout;
  }

  void AsyncResourceLockReq::onTimeoutExceeded( Timer & )
  {
    _sigTimeout.emit(*this);
    setFinished( expected<ResourceLock>::error( ZYPP_EXCPT_PTR( ResourceLockTimeoutException(_ident) ) ) );
  }

  ResourceLock::ResourceLock(detail::ResourceLockData_Ptr data)
    : _lockData( std::move(data) )
  {
  }

  ContextBaseWeakRef ResourceLock::lockContext() const
  {
    return _lockData->_zyppContext;
  }

  const std::string &ResourceLock::lockIdent() const
  {
    return _lockData->_resourceIdent;
  }

  ResourceLock::Mode ResourceLock::mode() const
  {
    return _lockData->_mode;
  }

}
