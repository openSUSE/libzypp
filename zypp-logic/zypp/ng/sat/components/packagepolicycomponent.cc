/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "packagepolicycomponent.h"
#include <zypp/ng/sat/pool.h>
#include <zypp/ng/sat/solvable.h>
#include <zypp/ng/sat/capability.h>

extern "C"
{
#include <solv/pool.h>
}

namespace zyppng::sat {

  bool PackagePolicyComponent::isRetracted( const Solvable & solv_r ) const
  {
    // Lazy initialization would require a pool reference, but we can assume
    // it's initialized during prepare or on first access if we pass the pool.
    // For now, let's assume it's synced via prepare/onInvalidate.
    return _retractedSpec.count( solv_r ) > 0;
  }

  bool PackagePolicyComponent::isPtfMaster( const Solvable & solv_r ) const
  {
    return _ptfMasterSpec.count( solv_r ) > 0;
  }

  bool PackagePolicyComponent::isPtfPackage( const Solvable & solv_r ) const
  {
    return _ptfPackageSpec.count( solv_r ) > 0;
  }

#if 0
  void PackagePolicyComponent::setNeedrebootSpec( SolvableSpec spec )
  {
    // Need to handle SolvableSpec -> std::set<Solvable> conversion or
    // implement SolvableSpec in zyppng.
    // For now, let's just store the placeholder logic.
  }
#endif

  void PackagePolicyComponent::onInvalidate( Pool & pool, PoolInvalidation invalidation )
  {
    if ( invalidation == PoolInvalidation::Data )
    {
      _initialized = false;
      _retractedSpec.clear();
      _ptfMasterSpec.clear();
      _ptfPackageSpec.clear();
    }
  }

  void PackagePolicyComponent::ensureInitialized( Pool & pool ) const
  {
    if ( _initialized )
      return;

    // Implementation of whatprovides-based spec initialization
    // using libsolv primitives directly since zyppng::sat::WhatProvides
    // might not be available yet.

    auto collect = [&]( IdString token, std::set<Solvable> & dest ) {
        detail::IdType cap = token.id();
        unsigned offset = ::pool_whatprovides( pool.get(), cap );
        detail::IdType * p = pool.get()->whatprovidesdata + offset;
        for ( ; *p; ++p ) {
            dest.insert( Solvable(*p) );
        }
    };

    collect( Solvable::retractedToken, _retractedSpec );
    collect( Solvable::ptfMasterToken, _ptfMasterSpec );
    collect( Solvable::ptfPackageToken, _ptfPackageSpec );

    _initialized = true;
  }

} // namespace zyppng::sat
