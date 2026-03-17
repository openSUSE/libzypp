/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "preparedpool.h"
#include <zypp/ng/sat/pool.h>
#include <zypp/ng/sat/capability.h>
#include <zypp/ng/sat/solvable.h>

#ifndef NDEBUG
#  include <cassert>
#endif

namespace zyppng::sat {

  PreparedPool::PreparedPool( Pool & pool_r ) noexcept
    : _pool( pool_r )
#ifndef NDEBUG
    , _serialWatcher()
#endif
  {
#ifndef NDEBUG
    _serialWatcher.remember( _pool.serial() );
#endif
  }

  detail::CPool * PreparedPool::get() const noexcept
  {
#ifndef NDEBUG
    assert( !_serialWatcher.remember( _pool.serial() )
            && "PreparedPool used after Pool was invalidated" );
#endif
    return _pool.get();
  }

  unsigned PreparedPool::whatProvidesCapabilityId( detail::IdType cap_r ) const
  {
    return ::pool_whatprovides( get(), cap_r );
  }

  detail::IdType PreparedPool::whatProvidesData( unsigned offset_r ) const
  {
    return get()->whatprovidesdata[offset_r];
  }

  Queue PreparedPool::whatMatchesDep( const SolvAttr & attr, const Capability & cap ) const
  {
    sat::Queue q;
    ::pool_whatmatchesdep( get(), attr.id(), cap.id(), q, 0 );
    return q;
  }

  Queue PreparedPool::whatMatchesSolvable( const SolvAttr & attr, const Solvable & solv ) const
  {
    sat::Queue q;
    ::pool_whatmatchessolvable( get(), attr.id(), static_cast<Id>( solv.id() ), q, 0 );
    return q;
  }

  Queue PreparedPool::whatContainsDep( const SolvAttr & attr, const Capability & cap ) const
  {
    sat::Queue q;
    ::pool_whatcontainsdep( get(), attr.id(), cap.id(), q, 0 );
    return q;
  }

} // namespace zyppng::sat
