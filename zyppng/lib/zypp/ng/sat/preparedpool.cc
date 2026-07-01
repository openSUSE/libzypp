/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
module;
#include <zypp/ng/sat/solvincludes.h>
#ifndef NDEBUG
#  include <cassert>
#endif

module zyppng;

import :sat_preparedpool;
import :sat_pool;
import :sat_capability;
import :sat_solvable;

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
    // _serialWatcher.remember() returns true when the serial has changed
    // since the last call — i.e. the pool was invalidated. We assert it
    // has NOT changed: a PreparedPool must not be used after pool invalidation.
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
    // libsolv guarantees null-termination of whatprovidesdata[]; callers
    // must stop iterating at the first 0 entry. The assert below catches
    // out-of-bounds access in debug builds by checking that the offset
    // does not exceed the next free slot (whatprovidesdataoff).
#ifndef NDEBUG
    assert( offset_r < static_cast<unsigned>( get()->whatprovidesdataoff )
            && "whatProvidesData offset out of bounds" );
#endif
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
