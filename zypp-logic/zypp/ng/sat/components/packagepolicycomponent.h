/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/**
 * This file contains private API, this might break at any time between releases.
 * You have been warned!
 */
#ifndef ZYPP_NG_SAT_COMPONENTS_PACKAGEPOLICYCOMPONENT_H_INCLUDED
#define ZYPP_NG_SAT_COMPONENTS_PACKAGEPOLICYCOMPONENT_H_INCLUDED

#include "poolcomponents.h"
#include <zypp/ng/sat/solvablespec.h>
#include <zypp/ng/sat/solvable.h>
#include <optional>

namespace zyppng::sat {

  /**
   * \class PackagePolicyComponent
   * \brief Component managing package-level policies: retracted packages,
   *        PTF tracking, and the "reboot needed" hint.
   *
   * \par Design
   * Each policy is represented by two pieces of state:
   *  - A \ref SolvableSpec (pure data: idents + provides tokens). This is
   *    the \e definition of the set and is what callers configure via
   *    \c setNeedrebootSpec().  The retracted/PTF specs are hardwired.
   *  - An \c std::optional<EvaluatedSolvableSpec> that is constructed in
   *    \c prepare(Pool&) and reset to \c nullopt by \c onInvalidate().
   *    Once evaluated, \ref contains() is O(1) with no pool access.
   *
   * \par Lifecycle
   * - \c prepare(Pool&) is called by the Pool during its own prepare() cycle.
   *   This is where the specs are expanded and the evaluated forms constructed.
   * - \c onInvalidate() resets the evaluated forms so they are rebuilt on the
   *   next \c prepare() call.
   *
   * \note The hardwired retracted/PTF provides tokens are the same strings
   *       used in the legacy \c zypp::sat::Pool:
   *       \c "retracted-patch-package()", \c "ptf()", \c "ptf-package()".
   */
  class PackagePolicyComponent : public IPoolComponent
  {
  public:
    PackagePolicyComponent();
    ~PackagePolicyComponent() override = default;

    ComponentStage stage() const override { return ComponentStage::Policy; }

    /** Called by Pool::prepare() — evaluates all specs against the current pool. */
    void prepare( Pool & pool ) override;

    /** Called when pool content changes — resets all evaluated forms. */
    void onInvalidate( Pool & pool, PoolInvalidation invalidation ) override;

    /** \name Retracted packages */
    //@{
    bool isRetracted( const Solvable & solv_r ) const;
    //@}

    /** \name PTF (Program Temporary Fix) */
    //@{
    bool isPtfMaster ( const Solvable & solv_r ) const;
    bool isPtfPackage( const Solvable & solv_r ) const;
    //@}

    /** \name Reboot-needed hint */
    //@{
    /**
     * \brief Replace the reboot-needed spec.
     *
     * The spec is evaluated on the next \c prepare() call.
     * Pass an empty \ref SolvableSpec to disable the hint entirely.
     */
    void setNeedrebootSpec( SolvableSpec spec );

    bool isNeedreboot( const Solvable & solv_r ) const;
    //@}

  private:
    // --- spec (definition) layer ---
    SolvableSpec _retractedSpec;   ///< hardwired: provides retracted-patch-package()
    SolvableSpec _ptfMasterSpec;   ///< hardwired: provides ptf()
    SolvableSpec _ptfPackageSpec;  ///< hardwired: provides ptf-package()
    SolvableSpec _needrebootSpec;  ///< user-supplied

    // --- evaluated (cache) layer ---
    // Populated by prepare(); reset to nullopt by onInvalidate().
    std::optional<EvaluatedSolvableSpec> _retractedEval;
    std::optional<EvaluatedSolvableSpec> _ptfMasterEval;
    std::optional<EvaluatedSolvableSpec> _ptfPackageEval;
    std::optional<EvaluatedSolvableSpec> _needrebootEval;
  };

} // namespace zyppng::sat

#endif // ZYPP_NG_SAT_COMPONENTS_PACKAGEPOLICYCOMPONENT_H_INCLUDED
