/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPP_NG_SAT_COMPONENTS_PACKAGEPOLICYCOMPONENT_H_INCLUDED
#define ZYPP_NG_SAT_COMPONENTS_PACKAGEPOLICYCOMPONENT_H_INCLUDED

#include "poolcomponents.h"
#include <zypp/ng/sat/solvable.h>
#include <set>

namespace zyppng::sat {
  /**
   * \brief Component managing various package-level policies and blacklists.
   *
   * This includes:
   * - Retracted packages (hidden from the user)
   * - PTF (Program Temporary Fix) master and package tracking
   * - Solvables triggering a "reboot needed" hint
   *
   * \note I decided to use one component for multiple specs here,
   *       but if we realize one of them becomes more complex, we can still factor it out
   */
  class PackagePolicyComponent : public IPoolComponent {
    public:
      PackagePolicyComponent() = default;

      ComponentStage stage() const override { return ComponentStage::Policy; }

      /** \name Blacklisted Solvables. */
      //@{
      bool isRetracted( const Solvable & solv_r ) const;
      //@}

      /** \name PTF Support */
      //@{
      bool isPtfMaster( const Solvable & solv_r ) const;
      bool isPtfPackage( const Solvable & solv_r ) const;
      //@}

      /** \name Reboot Needed Hint */
      //@{
      void setNeedrebootSpec( std::set<Solvable> spec ) { _needrebootSpec = std::move(spec); }
      bool isNeedreboot( const Solvable & solv_r ) const { return _needrebootSpec.count(solv_r) > 0; }
      //@}

      void onInvalidate( Pool & pool, PoolInvalidation invalidation ) override;

    private:
      void ensureInitialized( Pool & pool ) const;

    private:
      mutable std::set<Solvable> _retractedSpec;
      mutable std::set<Solvable> _ptfMasterSpec;
      mutable std::set<Solvable> _ptfPackageSpec;
      std::set<Solvable> _needrebootSpec;
      mutable bool _initialized = false;
  };

} // namespace zyppng::sat

#endif
