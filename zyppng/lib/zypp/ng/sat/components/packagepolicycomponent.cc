/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
module;
#include <cassert>
#include <optional>

module zyppng;

import :sat_components_packagepolicycomponent;
import :sat_preparedpool;
import :sat_pool;
import :sat_solvable;
import :sat_capability;

namespace zyppng::sat {

  // -----------------------------------------------------------------------
  // Construction: wire up the hardwired provides tokens
  // -----------------------------------------------------------------------

  // -----------------------------------------------------------------------
  // Construction: wire up the hardwired provides tokens
  // -----------------------------------------------------------------------

  PackagePolicyComponent::PackagePolicyComponent()
  {
    // Retracted: provides retracted-patch-package()
    Capability retractedCap( Solvable::retractedToken.c_str(), Capability::PARSED );
    _retractedSpec.addProvides( retractedCap );

    // PTF master: provides ptf()
    Capability ptfMasterCap( Solvable::ptfMasterToken.c_str(), Capability::PARSED );
    _ptfMasterSpec.addProvides( ptfMasterCap );

    // PTF package: provides ptf-package()
    Capability ptfPackageCap( Solvable::ptfPackageToken.c_str(), Capability::PARSED );
    _ptfPackageSpec.addProvides( ptfPackageCap );
  }

  // -----------------------------------------------------------------------
  // IPreparedPoolComponent overrides
  // -----------------------------------------------------------------------

  void PackagePolicyComponent::attach( Pool &p )
  {
    assert( _pool == nullptr && "PackagePolicyComponent can only be attached once");
    _pool = &p;
  }

  void PackagePolicyComponent::prepare( PreparedPool & pp )
  {
    _retractedEval .emplace( pp, _retractedSpec  );
    _ptfMasterEval .emplace( pp, _ptfMasterSpec  );
    _ptfPackageEval.emplace( pp, _ptfPackageSpec );
    _needrebootEval.emplace( pp, _needrebootSpec );
  }

  void PackagePolicyComponent::onInvalidate( Pool & /*pool*/, PoolInvalidation invalidation )
  {
    if ( invalidation == PoolInvalidation::Data ) {
      _retractedEval .reset();
      _ptfMasterEval .reset();
      _ptfPackageEval.reset();
      _needrebootEval.reset();
      // Note: the specs themselves (definitions) are kept; only the
      // evaluated forms are dropped.  prepare() will rebuild them.
    }
  }

  void PackagePolicyComponent::onReset( Pool & )
  {
    _retractedEval .reset();
    _ptfMasterEval .reset();
    _ptfPackageEval.reset();
    _needrebootEval.reset();
  }

  // -----------------------------------------------------------------------
  // Public query API
  // -----------------------------------------------------------------------

  bool PackagePolicyComponent::isRetracted( const Solvable & solv_r ) const
  { return _retractedEval && _retractedEval->contains( solv_r ); }

  bool PackagePolicyComponent::isPtfMaster( const Solvable & solv_r ) const
  { return _ptfMasterEval && _ptfMasterEval->contains( solv_r ); }

  bool PackagePolicyComponent::isPtfPackage( const Solvable & solv_r ) const
  { return _ptfPackageEval && _ptfPackageEval->contains( solv_r ); }

  void PackagePolicyComponent::setNeedrebootSpec( SolvableSpec spec )
  {
    _needrebootSpec = std::move( spec );
    // The evaluated form is stale now; it will be rebuilt on next prepare().
    _needrebootEval.reset();

    if ( _pool )
      _pool->setDirty( PoolInvalidation::Dependency, { __PRETTY_FUNCTION__, "setting new reboot spec"} );
  }

  bool PackagePolicyComponent::isNeedreboot( const Solvable & solv_r ) const
  { return _needrebootEval && _needrebootEval->contains( solv_r ); }

} // namespace zyppng::sat
