/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/Resolver.h
 *
*/
#ifndef ZYPP_RESOLVER_H
#define ZYPP_RESOLVER_H

#include <iosfwd>
#include <functional>

#include "zypp/base/ReferenceCounted.h"
#include "zypp/base/PtrTypes.h"

#include "zypp/ResPool.h"
#include "zypp/UpgradeStatistics.h"
#include "zypp/solver/detail/Resolver.h"
#include "zypp/solver/detail/SolverQueueItem.h"
#include "zypp/ProblemTypes.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////


  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : Resolver
  //
  /**
   * Dependency resolver interface.
   *
   * To resolve dependencies after making changes to the \ref ResPool (using
   * \ref addRequire(), \ref addConflict(), \ref applySolutions(), or by making
   * the changes directly on the \ref PoolItem status objects,
   * call the \ref resolvePool() method.
   */
  class Resolver : public base::ReferenceCounted, private base::NonCopyable
  {
  public:

    /** Ctor */
    Resolver( const ResPool & pool );
    /** Dtor */
    virtual ~Resolver();

    /**
     * Resolve package dependencies:
     *
     * Enter \ref systemVerification mode to monitor and repair dependencies
     * of already installed packages, and solve immediately.
     *
     * Call \ref setSystemVerification to turn of this mode.
     **/
    bool verifySystem();


    /**
     * Resolve package dependencies:
     *
     * Try to execute all pending transactions (there may be more than
     * one!).
     * The solver collects all transactions (install/delete resolvables)
     * from the pool, generates task, solving it and writes the
     * results back to pool
     *
     * Returns "true" on success (i.e., if there were no problems that
     * need user interaction) and "false" if there were problems.  In
     * the latter case, use problems() and later applySolutions()
     * below.
     **/
    bool resolvePool();


    /**
     * Resolve package dependencies:
     *
     * The solver works off the given queue and writes back the solution
     * to pool.
     *
     * Returns "true" on success (i.e., if there were no problems that
     * need user interaction) and "false" if there were problems.  In
     * the latter case, use problems() and later applySolutions()
     * below.
     * The solution could be that the solver remove/add some entries
     * in the task queue. So make a new call of resolveQueue after you
     * have applied any solution AND check the parameter "queue" if
     * there has been any changes by the solver and adapt these changes
     * to e.g. the selectables.
     *
     **/
    bool resolveQueue( solver::detail::SolverQueueItemList & queue );

    /*
     * Undo solver changes done in resolvePool()
     * Throwing away all ignored dependencies.
     */
    void undo();

    /*
     * Resets solver information and verify option.
     */
    void reset();


    /**
     * Do an distribution upgrade
     *
     * This will run a full upgrade on the pool, taking all upgrade
     * dependencies (provide/obsolete for package renames, split-
     * provides, etc.) into account and actually removing installed
     * packages if no upgrade exists AND the package dependency is
     * broken
     *
     * To be run with great caution. It basically brings your
     * system 'back to start'.
     * Quite helpful to get back to a 'sane state'. Quite disastrous
     * since you'll loose all non-distribution packages
     **/
    bool doUpgrade( UpgradeStatistics & opt_stats_r );

    /**
     * Update to newest package
     *
     * Install the newest version of your installed packages as
     * far as possible. This means a newer package will NOT be
     * installed if it generates dependency problems.
     * So the user will not get an error message.
     *
     **/
    void doUpdate( );


    /**
     * Unmaintained packages which does not fit to
     * the updated system (broken dependencies) will be
     * deleted.
     * Return the list of deleted items.
     * Note : This list is valid after the call doUpgrade() only.
     **/
    std::list<PoolItem> problematicUpdateItems() const;

    /**
     * Return the dependency problems found by the last call to
     * resolveDependencies(). If there were no problems, the returned
     * list will be empty.
     **/
    ResolverProblemList problems();


    /**
     * Apply problem solutions. No more than one solution per problem
     * can be applied.
     **/
    void applySolutions( const ProblemSolutionList & solutions );


    /**
     * Remove resolvables which are conflicts with others or
     * have unfulfilled requirements.
     * This behaviour is favourited by ZMD.
     **/
    void setForceResolve( bool force );
    bool forceResolve();

    /**
     * Ignore recommended packages that were already recommended by
     * the installed packages
     **/
    void setIgnoreAlreadyRecommended( bool yesno_r );
    bool ignoreAlreadyRecommended();

    /**
     * Setting whether required packages are installed ONLY
     * So recommended packages, language packages and packages which depend
     * on hardware (modalias) will not be regarded.
     **/
    void setOnlyRequires( bool yesno_r );
    void resetOnlyRequires(); // set back to default (described in zypp.conf)
    bool onlyRequires();

    /**
     * Setting whether the solver should allow or disallow vendor changes.
     *
     * If OFF (the default) the solver will replace packages with packages
     * of the same (or equivalent) vendor ony.
     *
     * \see \ref VendorAttr for definition of vendor equivalence.
     **/
    void setAllowVendorChange( bool yesno_r );
    void setDefaultAllowVendorChange(); // set back to default (in zypp.conf)
    bool allowVendorChange() const;

    /**
     * System verification mode also monitors and repairs dependencies
     * of already installed packages.
     * \see \ref verifySystem
     */
    void setSystemVerification( bool yesno_r );
    void setDefaultSystemVerification();
    bool systemVerification() const;

    /**
     * Set whether to solve source packages build dependencies per default.
     * Usually turned off and if, enabled per source package.
     * \NOTE This affects only source packges selected in the \ref ResPool. No solver rule
     * will be generated for them. Source packages requested via e.g. \ref addRequire will
     * always be solved.
     * \NOTE Be carefull. The older the source package is, the stranger may be the
     * result of solving it's build dependencies.
     */
    void setSolveSrcPackages( bool yesno_r );
    void setDefaultSolveSrcPackages();
    bool solveSrcPackages() const;

    /**
     * Adding additional requirement
     *
     */
    void addRequire( const Capability & capability );

    /**
     * Adding additional conflict
     *
     */
    void addConflict( const Capability & capability );

    /**
     * Remove the additional requirement set by \ref addRequire(Capability).
     *
     */
    void removeRequire( const Capability & capability );

    /**
     * Remove the additional conflict set by \ref addConflict(Capability).
     *
     */
    void removeConflict( const Capability & capability );

    /**
     * Get all the additional requirements set by \ref addRequire(Capability).
     *
     */
    CapabilitySet getRequire();

    /**
     * Get all the additional conflicts set by \ref addConflict(Capability).
     *
     */
    CapabilitySet getConflict();

    /**
     * Generates a solver Testcase of the current state
     *
     * \parame dumpPath destination directory of the created directory
     * \return true if it was successful
     */
    bool createSolverTestcase( const std::string & dumpPath = "/var/log/YaST2/solverTestcase" );

    /**
     * Gives information about WHO has pused an installation of an given item.
     *
     * \param item    Evaluate additional information for this resolvable.
     * \return A list of structures which contains:
     *		item                Item which has triggered the installation of the given param item.
     *          initialInstallation This item has triggered the installation
     *	                            Not already fullfilled requierement only.
     *		cap                 Capability which has triggerd this installation
     *		capKind             Kind of that capability (e.g.  Dep::REQUIRES,Dep::RECOMMENDS,... )
     *
     * Note: In order to have a result start a solver run before. Not matter if it is valid or invalid.
     *
     */
    solver::detail::ItemCapKindList isInstalledBy( const PoolItem & item );

    /**
     * Gives information about WHICH additional items will be installed due the installation of an item.
     *
     * \param item     Evaluate additional information for this resolvable.
     * \return A list of structures which contains:
     *		item                Item which has triggered the installation of the given param item.
     *          initialInstallation This item has triggered the installation
     *	                            Not already fullfilled requierement only.
     *		cap                 Capability which has triggerd this installation
     *		capKind             Kind of that capability (e.g.  Dep::REQUIRES,Dep::RECOMMENDS,... )
     *
     * Note: In order to have a result start a solver run before. Not matter if it is valid or invalid.
     *
     */
    solver::detail::ItemCapKindList installs( const PoolItem & item );

    /**
     * Gives information about WHICH installed items are requested by the installation of an item.
     *
     * \param item     Evaluate additional information for this resolvable.
     * \return A list of structures which contains:
     *		item                Item which has triggered the installation of the given param item.
     *          initialInstallation This item has triggered the installation
     *	                            Not already fullfilled requierement only.
     *		cap                 Capability which has triggerd this installation
     *		capKind             Kind of that capability (e.g.  Dep::REQUIRES,Dep::RECOMMENDS,... )
     *
     * Note: In order to have a result start a solver run before. Not matter if it is valid or invalid.
     *
     */
    solver::detail::ItemCapKindList satifiedByInstalled( const PoolItem & item );


    /**
     * Gives information about WHICH items require an already installed item.
     *
     * \param item     Evaluate additional information for this resolvable.
     * \return A list of structures which contains:
     *		item                Item which has triggered the installation of the given param item.
     *          initialInstallation This item has triggered the installation
     *	                            Not already fullfilled requierement only.
     *		cap                 Capability which has triggerd this installation
     *		capKind             Kind of that capability (e.g.  Dep::REQUIRES,Dep::RECOMMENDS,... )
     *
     * Note: In order to have a result start a solver run before. Not matter if it is valid or invalid.
     *
     */
    solver::detail::ItemCapKindList installedSatisfied( const PoolItem & item );



  private:
    solver::detail::Resolver_Ptr _pimpl;
  };
  ///////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_RESOLVER_H
