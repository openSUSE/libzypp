/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* Resolver.cc
 *
 * Copyright (C) 2000-2002 Ximian, Inc.
 * Copyright (C) 2005 SUSE Linux Products GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA.
 */
#include <boost/static_assert.hpp>
#include <utility>

#define ZYPP_USE_RESOLVER_INTERNALS

#include <zypp/base/LogTools.h>
#include <zypp/base/Algorithm.h>

#include <zypp/solver/detail/Resolver.h>
#include <zypp/solver/detail/Testcase.h>
#include <zypp/solver/detail/SATResolver.h>
#include <zypp/solver/detail/ItemCapKind.h>
#include <zypp/solver/detail/SolutionAction.h>
#include <zypp/solver/detail/SolverQueueItem.h>

#include <zypp/ZConfig.h>
#include <zypp/sat/Transaction.h>

#define MAXSOLVERRUNS 5

using std::endl;
using std::make_pair;

#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::solver"

/////////////////////////////////////////////////////////////////////////
namespace zypp
{ ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
  namespace solver
  { /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
    namespace detail
    { ///////////////////////////////////////////////////////////////////


//---------------------------------------------------------------------------


std::ostream & Resolver::dumpOn( std::ostream & os ) const
{
  os << "<resolver>" << endl;
  #define OUTS(t) os << "  " << #t << ":\t" << t << endl;
  OUTS( _upgradeMode );
  OUTS( _updateMode );
  OUTS( _verifying );
  OUTS( _solveSrcPackages );
  OUTS( _ignoreAlreadyRecommended );
  #undef OUT
  return os << "<resolver/>";
}


//---------------------------------------------------------------------------

Resolver::Resolver (ResPool  pool)
    : _pool(std::move(pool))
    , _satResolver(NULL)
    , _poolchanged(_pool.serial() )
    , _upgradeMode              ( false )
    , _updateMode               ( false )
    , _verifying                ( false )
    , _solveSrcPackages         ( false )
    , _ignoreAlreadyRecommended ( true )
    , _applyDefault_focus                ( true )
    , _applyDefault_forceResolve         ( true )
    , _applyDefault_cleandepsOnRemove    ( true )
    , _applyDefault_onlyRequires         ( true )
    , _applyDefault_allowDowngrade       ( true )
    , _applyDefault_allowNameChange      ( true )
    , _applyDefault_allowArchChange      ( true )
    , _applyDefault_allowVendorChange    ( true )
    , _applyDefault_dupAllowDowngrade    ( true )
    , _applyDefault_dupAllowNameChange   ( true )
    , _applyDefault_dupAllowArchChange   ( true )
    , _applyDefault_dupAllowVendorChange ( true )
{
    sat::Pool satPool( sat::Pool::instance() );
    _satResolver = new SATResolver(_pool, satPool.get());
}


Resolver::~Resolver()
{
  delete _satResolver;
}

sat::detail::CSolver * Resolver::get() const
{ return _satResolver->get(); }


void Resolver::setDefaultSolverFlags( bool all_r )
{
  MIL << "setDefaultSolverFlags all=" << all_r << endl;

  if ( all_r || _applyDefault_focus ) setFocus( ResolverFocus::Default );

#define ZOLV_FLAG_DEFAULT( ZSETTER, ZGETTER )        \
  if ( all_r || _applyDefault_##ZGETTER ) ZSETTER( indeterminate )

  ZOLV_FLAG_DEFAULT( setForceResolve         ,forceResolve         );
  ZOLV_FLAG_DEFAULT( setCleandepsOnRemove    ,cleandepsOnRemove    );
  ZOLV_FLAG_DEFAULT( setOnlyRequires         ,onlyRequires         );
  ZOLV_FLAG_DEFAULT( setAllowDowngrade       ,allowDowngrade       );
  ZOLV_FLAG_DEFAULT( setAllowNameChange      ,allowNameChange      );
  ZOLV_FLAG_DEFAULT( setAllowArchChange      ,allowArchChange      );
  ZOLV_FLAG_DEFAULT( setAllowVendorChange    ,allowVendorChange    );
  ZOLV_FLAG_DEFAULT( dupSetAllowDowngrade    ,dupAllowDowngrade    );
  ZOLV_FLAG_DEFAULT( dupSetAllowNameChange   ,dupAllowNameChange   );
  ZOLV_FLAG_DEFAULT( dupSetAllowArchChange   ,dupAllowArchChange   );
  ZOLV_FLAG_DEFAULT( dupSetAllowVendorChange ,dupAllowVendorChange );
#undef ZOLV_FLAG_TRIBOOL
}
//---------------------------------------------------------------------------
// forward flags too SATResolver
void Resolver::setFocus( ResolverFocus focus_r )	{
  _applyDefault_focus = ( focus_r == ResolverFocus::Default );
  _satResolver->_focus = _applyDefault_focus ? ZConfig::instance().solver_focus() : focus_r;
}
ResolverFocus Resolver::focus() const			{ return _satResolver->_focus; }

void Resolver::setRemoveOrphaned( bool yesno_r )        { _satResolver->_removeOrphaned = yesno_r; }
bool Resolver::removeOrphaned() const                   { return _satResolver->_removeOrphaned; }

void Resolver::setRemoveUnneeded( bool yesno_r )        { _satResolver->_removeUnneeded = yesno_r; }
bool Resolver::removeUnneeded() const                   { return _satResolver->_removeUnneeded; }

#define ZOLV_FLAG_TRIBOOL( ZSETTER, ZGETTER, ZVARDEFAULT, ZVARNAME )			\
    void Resolver::ZSETTER( TriBool state_r )						\
    { _applyDefault_##ZGETTER = indeterminate(state_r);					\
      bool newval = _applyDefault_##ZGETTER ? ZVARDEFAULT : bool(state_r);		\
      if ( ZVARNAME != newval ) {							\
        DBG << #ZGETTER << ": changed from " << (bool)ZVARNAME << " to " << newval << endl;\
        ZVARNAME = newval;								\
      }											\
    }											\
    bool Resolver::ZGETTER() const							\
    { return ZVARNAME; }								\

// Flags stored here follow the naming convention,...
// Flags down in _satResolver don't. Could match `_satResolver->_##ZGETTER`
#define ZOLV_FLAG_SATSOLV( ZSETTER, ZGETTER, ZVARDEFAULT, ZVARNAME )			\
    ZOLV_FLAG_TRIBOOL( ZSETTER, ZGETTER, ZVARDEFAULT, _satResolver->ZVARNAME )

// NOTE: ZVARDEFAULT must be in sync with SATResolver ctor
ZOLV_FLAG_SATSOLV( setForceResolve         ,forceResolve         ,false                                             ,_allowuninstall        )
ZOLV_FLAG_SATSOLV( setCleandepsOnRemove    ,cleandepsOnRemove    ,ZConfig::instance().solver_cleandepsOnRemove()    ,_cleandepsOnRemove     )
ZOLV_FLAG_SATSOLV( setOnlyRequires         ,onlyRequires         ,ZConfig::instance().solver_onlyRequires()         ,_onlyRequires          )
ZOLV_FLAG_SATSOLV( setAllowDowngrade       ,allowDowngrade       ,false                                             ,_allowdowngrade        )
ZOLV_FLAG_SATSOLV( setAllowNameChange      ,allowNameChange      ,true /*bsc#1071466*/                              ,_allownamechange       )
ZOLV_FLAG_SATSOLV( setAllowArchChange      ,allowArchChange      ,false                                             ,_allowarchchange       )
ZOLV_FLAG_SATSOLV( setAllowVendorChange    ,allowVendorChange    ,ZConfig::instance().solver_allowVendorChange()    ,_allowvendorchange     )
ZOLV_FLAG_SATSOLV( dupSetAllowDowngrade    ,dupAllowDowngrade    ,ZConfig::instance().solver_dupAllowDowngrade()    ,_dup_allowdowngrade    )
ZOLV_FLAG_SATSOLV( dupSetAllowNameChange   ,dupAllowNameChange   ,ZConfig::instance().solver_dupAllowNameChange()   ,_dup_allownamechange   )
ZOLV_FLAG_SATSOLV( dupSetAllowArchChange   ,dupAllowArchChange   ,ZConfig::instance().solver_dupAllowArchChange()   ,_dup_allowarchchange   )
ZOLV_FLAG_SATSOLV( dupSetAllowVendorChange ,dupAllowVendorChange ,ZConfig::instance().solver_dupAllowVendorChange() ,_dup_allowvendorchange )
#undef ZOLV_FLAG_SATSOLV
#undef ZOLV_FLAG_TRIBOOL
//---------------------------------------------------------------------------

ResPool Resolver::pool() const
{ return _pool; }

void Resolver::reset( bool keepExtras )
{
    _verifying = false;

    if (!keepExtras) {
      _extra_requires.clear();
      _extra_conflicts.clear();
    }

    _isInstalledBy.clear();
    _installs.clear();
    _satifiedByInstalled.clear();
    _installedSatisfied.clear();
}

PoolItemList Resolver::problematicUpdateItems() const
{ return _satResolver->problematicUpdateItems(); }

void Resolver::addExtraRequire( const Capability & capability )
{ _extra_requires.insert (capability); }

void Resolver::removeExtraRequire( const Capability & capability )
{ _extra_requires.erase (capability); }

void Resolver::addExtraConflict( const Capability & capability )
{ _extra_conflicts.insert (capability); }

void Resolver::removeExtraConflict( const Capability & capability )
{ _extra_conflicts.erase (capability); }

void Resolver::removeQueueItem( const SolverQueueItem_Ptr& item )
{
    bool found = false;
    for (SolverQueueItemList::const_iterator iter = _added_queue_items.begin();
         iter != _added_queue_items.end(); iter++) {
        if (*iter == item) {
            _added_queue_items.remove(*iter);
            found = true;
            break;
        }
    }
    if (!found) {
        _removed_queue_items.push_back (item);
        _removed_queue_items.unique ();
    }
}

void Resolver::addQueueItem( const SolverQueueItem_Ptr& item )
{
    bool found = false;
    for (SolverQueueItemList::const_iterator iter = _removed_queue_items.begin();
         iter != _removed_queue_items.end(); iter++) {
        if (*iter == item) {
            _removed_queue_items.remove(*iter);
            found = true;
            break;
        }
    }
    if (!found) {
        _added_queue_items.push_back (item);
        _added_queue_items.unique ();
    }
}

void Resolver::addWeak( const PoolItem & item )
{ _addWeak.push_back( item ); }

//---------------------------------------------------------------------------

struct UndoTransact
{
    ResStatus::TransactByValue resStatus;
    UndoTransact ( const ResStatus::TransactByValue &status)
        :resStatus(status)
    { }

    bool operator()( const PoolItem& item )		// only transacts() items go here
    {
        item.status().resetTransact( resStatus );// clear any solver/establish transactions
        return true;
    }
};


struct DoTransact
{
    ResStatus::TransactByValue resStatus;
    DoTransact ( const ResStatus::TransactByValue &status)
        :resStatus(status)
    { }

    bool operator()( const PoolItem& item )		// only transacts() items go here
    {
        item.status().setTransact( true, resStatus );
        return true;
    }
};

//----------------------------------------------------------------------------
/// \brief Write automatic testcases if ZYPP_FULLLOG=1 is set
///
/// As a matter of fact the testcase writer needs a \ref Resolver but can not
/// write the testcase unless the underlying \ref SATResolver is initialized.
/// So we write out the testcase after solving and before Resolver ist left.
/// \ingroup g_RAII
struct ScopedAutoTestCaseWriter
{
  ScopedAutoTestCaseWriter( Resolver & resolver_r )
  {
    const char *val = ::getenv("ZYPP_FULLLOG");
    if ( val && str::strToTrue( val ) )
      _resolver = &resolver_r;
  }

  ~ScopedAutoTestCaseWriter()
  {
    if ( _resolver ) try {
        Testcase testcase( "/var/log/YaST2/autoTestcase" );
        testcase.createTestcase( *_resolver, dumpPool, false );
        if ( dumpPool )
          dumpPool = false;
    } catch( ... ) {};
  }

private:
  Resolver * _resolver = nullptr;
  static bool dumpPool; // dump pool on the 1st invocation, later update control file only
};

bool ScopedAutoTestCaseWriter::dumpPool = true;

//----------------------------------------------------------------------------
// undo
void Resolver::undo()
{
    UndoTransact info(ResStatus::APPL_LOW);
    MIL << "*** undo ***" << endl;
    invokeOnEach ( _pool.begin(), _pool.end(),
                   resfilter::ByTransact( ),			// collect transacts from Pool to resolver queue
                   std::ref(info) );
    //  Regard dependencies of the item weak onl
    _addWeak.clear();

    // Additional QueueItems which has to be regarded by the solver
    _removed_queue_items.clear();
    _added_queue_items.clear();

    return;
}

void Resolver::solverInit()
{
    // Solving with libsolv
    MIL << "-------------- Calling SAT Solver -------------------" << endl;

    // update solver mode flags
    _satResolver->setDistupgrade		(_upgradeMode);
    _satResolver->setUpdatesystem		(_updateMode);
    _satResolver->setFixsystem			( isVerifyingMode() );
    _satResolver->setSolveSrcPackages		( solveSrcPackages() );
    _satResolver->setIgnorealreadyrecommended	( ignoreAlreadyRecommended() );

    if (_upgradeMode) {
      // Maybe depend on ZConfig::solverUpgradeRemoveDroppedPackages.
      // For sure right but a change in behavior for old distros.
      // (Will disable weakremover processing in SATResolver)
      // _satResolver->setRemoveOrphaned( ... );
    }

    // Resetting additional solver information
    _isInstalledBy.clear();
    _installs.clear();
    _satifiedByInstalled.clear();
    _installedSatisfied.clear();
}

bool Resolver::verifySystem()
{
  DBG << "Resolver::verifySystem()" << endl;
  _verifying = true;
  UndoTransact resetting (ResStatus::APPL_HIGH);
  invokeOnEach ( _pool.begin(), _pool.end(),
                 resfilter::ByTransact( ),			// Resetting all transcations
                 std::ref(resetting) );
  return resolvePool();
}


bool Resolver::doUpgrade()
{
  // Setting Resolver to upgrade mode. SAT solver will do the update
  _upgradeMode = true;
  return resolvePool();
}

bool Resolver::resolvePool()
{
  ScopedAutoTestCaseWriter _raiiGuard( *this );  // Write a testcase if needed.
  solverInit();
  return _satResolver->resolvePool(_extra_requires, _extra_conflicts, _addWeak, _upgradeRepos );
}

void Resolver::doUpdate()
{
  ScopedAutoTestCaseWriter _raiiGuard( *this );  // Write a testcase if needed.
  _updateMode = true;
  solverInit();
  return _satResolver->doUpdate();
}

bool Resolver::resolveQueue( solver::detail::SolverQueueItemList & queue )
{
    ScopedAutoTestCaseWriter _raiiGuard( *this );  // Write a testcase if needed.
    solverInit();

    // add/remove additional SolverQueueItems
    for (SolverQueueItemList::const_iterator iter = _removed_queue_items.begin();
         iter != _removed_queue_items.end(); iter++) {
        for (SolverQueueItemList::const_iterator iterQueue = queue.begin(); iterQueue != queue.end(); iterQueue++) {
            if ( (*iterQueue)->cmp(*iter) == 0) {
                MIL << "remove from queue" << *iter;
                queue.remove(*iterQueue);
                break;
            }
        }
    }

    for (SolverQueueItemList::const_iterator iter = _added_queue_items.begin();
         iter != _added_queue_items.end(); iter++) {
        bool found = false;
        for (SolverQueueItemList::const_iterator iterQueue = queue.begin(); iterQueue != queue.end(); iterQueue++) {
            if ( (*iterQueue)->cmp(*iter) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            MIL << "add to queue" << *iter;
            queue.push_back(*iter);
        }
    }

    // The application has to take care to write these solutions back to e.g. selectables in order
    // give the user a chance for changing these decisions again.
    _removed_queue_items.clear();
    _added_queue_items.clear();

    return _satResolver->resolveQueue(queue, _addWeak);
}

sat::Transaction Resolver::getTransaction()
{
  // FIXME: That's an ugly way of pushing autoInstalled into the transaction.
  sat::Transaction ret( sat::Transaction::loadFromPool );
  ret.autoInstalled( _satResolver->autoInstalled() );
  return ret;
}


//----------------------------------------------------------------------------
// Getting more information about the solve results

ResolverProblemList Resolver::problems() const
{
  MIL << "Resolver::problems()" << endl;
  return _satResolver->problems();
}

void Resolver::applySolutions( const ProblemSolutionList & solutions )
{
  for ( const ProblemSolution_Ptr& solution : solutions )
  {
    if ( ! applySolution( *solution ) )
      break;
  }
}

bool Resolver::applySolution( const ProblemSolution & solution )
{
  bool ret = true;
  DBG << "apply solution " << solution << endl;
  for ( const SolutionAction_Ptr& action : solution.actions() )
  {
    if ( ! action->execute( *this ) )
    {
      WAR << "apply solution action failed: " << action << endl;
      ret = false;
      break;
    }
  }
  return ret;
}

//----------------------------------------------------------------------------

void Resolver::collectResolverInfo()
{
    if ( _satResolver
         && _isInstalledBy.empty()
         && _installs.empty()) {

        // generating new
        PoolItemList itemsToInstall = _satResolver->resultItemsToInstall();

        for (PoolItemList::const_iterator instIter = itemsToInstall.begin();
             instIter != itemsToInstall.end(); instIter++) {
            // Requires
            for (Capabilities::const_iterator capIt = (*instIter)->dep (Dep::REQUIRES).begin(); capIt != (*instIter)->dep (Dep::REQUIRES).end(); ++capIt)
            {
                sat::WhatProvides possibleProviders(*capIt);
                for_( iter, possibleProviders.begin(), possibleProviders.end() ) {
                    PoolItem provider = ResPool::instance().find( *iter );

                    // searching if this provider will already be installed
                    bool found = false;
                    bool alreadySetForInstallation = false;
                    ItemCapKindMap::const_iterator pos = _isInstalledBy.find(provider);
                    while (pos != _isInstalledBy.end()
                           && pos->first == provider
                           && !found) {
                        alreadySetForInstallation = true;
                        ItemCapKind capKind = pos->second;
                        if (capKind.item() == *instIter)  found = true;
                        pos++;
                    }

                    if (!found
                        && provider.status().isToBeInstalled()) {
                        if (provider.status().isBySolver()) {
                            ItemCapKind capKindisInstalledBy( *instIter, *capIt, Dep::REQUIRES, !alreadySetForInstallation );
                            _isInstalledBy.insert (make_pair( provider, capKindisInstalledBy));
                        } else {
                            // no initial installation cause it has been set be e.g. user
                            ItemCapKind capKindisInstalledBy( *instIter, *capIt, Dep::REQUIRES, false );
                            _isInstalledBy.insert (make_pair( provider, capKindisInstalledBy));
                        }
                        ItemCapKind capKindisInstalledBy( provider, *capIt, Dep::REQUIRES, !alreadySetForInstallation );
                        _installs.insert (make_pair( *instIter, capKindisInstalledBy));
                    }

                    if (provider.status().staysInstalled()) { // Is already satisfied by an item which is installed
                        ItemCapKind capKindisInstalledBy( provider, *capIt, Dep::REQUIRES, false );
                        _satifiedByInstalled.insert (make_pair( *instIter, capKindisInstalledBy));

                        ItemCapKind installedSatisfied( *instIter, *capIt, Dep::REQUIRES, false );
                        _installedSatisfied.insert (make_pair( provider, installedSatisfied));
                    }
                }
            }

            if (!(_satResolver->onlyRequires())) {
                //Recommends
                for (Capabilities::const_iterator capIt = (*instIter)->dep (Dep::RECOMMENDS).begin(); capIt != (*instIter)->dep (Dep::RECOMMENDS).end(); ++capIt)
                {
                    sat::WhatProvides possibleProviders(*capIt);
                    for_( iter, possibleProviders.begin(), possibleProviders.end() ) {
                        PoolItem provider = ResPool::instance().find( *iter );

                        // searching if this provider will already be installed
                        bool found = false;
                        bool alreadySetForInstallation = false;
                        ItemCapKindMap::const_iterator pos = _isInstalledBy.find(provider);
                        while (pos != _isInstalledBy.end()
                               && pos->first == provider
                               && !found) {
                            alreadySetForInstallation = true;
                            ItemCapKind capKind = pos->second;
                            if (capKind.item() == *instIter)  found = true;
                            pos++;
                        }

                        if (!found
                            && provider.status().isToBeInstalled()) {
                            if (provider.status().isBySolver()) {
                                ItemCapKind capKindisInstalledBy( *instIter, *capIt, Dep::RECOMMENDS, !alreadySetForInstallation );
                                _isInstalledBy.insert (make_pair( provider, capKindisInstalledBy));
                            } else {
                                // no initial installation cause it has been set be e.g. user
                                ItemCapKind capKindisInstalledBy( *instIter, *capIt, Dep::RECOMMENDS, false );
                                _isInstalledBy.insert (make_pair( provider, capKindisInstalledBy));
                            }
                            ItemCapKind capKindisInstalledBy( provider, *capIt, Dep::RECOMMENDS, !alreadySetForInstallation );
                            _installs.insert (make_pair( *instIter, capKindisInstalledBy));
                        }

                        if (provider.status().staysInstalled()) { // Is already satisfied by an item which is installed
                            ItemCapKind capKindisInstalledBy( provider, *capIt, Dep::RECOMMENDS, false );
                            _satifiedByInstalled.insert (make_pair( *instIter, capKindisInstalledBy));

                            ItemCapKind installedSatisfied( *instIter, *capIt, Dep::RECOMMENDS, false );
                            _installedSatisfied.insert (make_pair( provider, installedSatisfied));
                        }
                    }
                }

                //Supplements
                for (Capabilities::const_iterator capIt = (*instIter)->dep (Dep::SUPPLEMENTS).begin(); capIt != (*instIter)->dep (Dep::SUPPLEMENTS).end(); ++capIt)
                {
                    sat::WhatProvides possibleProviders(*capIt);
                    for_( iter, possibleProviders.begin(), possibleProviders.end() ) {
                        PoolItem provider = ResPool::instance().find( *iter );
                        // searching if this item will already be installed
                        bool found = false;
                        bool alreadySetForInstallation = false;
                        ItemCapKindMap::const_iterator pos = _isInstalledBy.find(*instIter);
                        while (pos != _isInstalledBy.end()
                               && pos->first == *instIter
                               && !found) {
                            alreadySetForInstallation = true;
                            ItemCapKind capKind = pos->second;
                            if (capKind.item() == provider)  found = true;
                            pos++;
                        }

                        if (!found
                            && instIter->status().isToBeInstalled()) {
                            if (instIter->status().isBySolver()) {
                                ItemCapKind capKindisInstalledBy( provider, *capIt, Dep::SUPPLEMENTS, !alreadySetForInstallation );
                                _isInstalledBy.insert (make_pair( *instIter, capKindisInstalledBy));
                            } else {
                                // no initial installation cause it has been set be e.g. user
                                ItemCapKind capKindisInstalledBy( provider, *capIt, Dep::SUPPLEMENTS, false );
                                _isInstalledBy.insert (make_pair( *instIter, capKindisInstalledBy));
                            }
                            ItemCapKind capKindisInstalledBy( *instIter, *capIt, Dep::SUPPLEMENTS, !alreadySetForInstallation );
                            _installs.insert (make_pair( provider, capKindisInstalledBy));
                        }

                        if (instIter->status().staysInstalled()) { // Is already satisfied by an item which is installed
                            ItemCapKind capKindisInstalledBy( *instIter, *capIt, Dep::SUPPLEMENTS, !alreadySetForInstallation );
                            _satifiedByInstalled.insert (make_pair( provider, capKindisInstalledBy));

                            ItemCapKind installedSatisfied( provider, *capIt, Dep::SUPPLEMENTS, false );
                            _installedSatisfied.insert (make_pair( *instIter, installedSatisfied));
                        }
                    }
                }
            }
        }
    }
}


ItemCapKindList Resolver::isInstalledBy( const PoolItem & item )
{
    ItemCapKindList ret;
    collectResolverInfo();

    for (ItemCapKindMap::const_iterator iter = _isInstalledBy.find(item); iter != _isInstalledBy.end();) {
        ItemCapKind info = iter->second;
        PoolItem iterItem = iter->first;
        if (iterItem == item) {
            ret.push_back(info);
            iter++;
        } else {
            // exit
            iter = _isInstalledBy.end();
        }
    }
    return ret;
}

ItemCapKindList Resolver::installs( const PoolItem & item )
{
    ItemCapKindList ret;
    collectResolverInfo();

    for (ItemCapKindMap::const_iterator iter = _installs.find(item); iter != _installs.end();) {
        ItemCapKind info = iter->second;
        PoolItem iterItem = iter->first;
        if (iterItem == item) {
            ret.push_back(info);
            iter++;
        } else {
            // exit
            iter = _installs.end();
        }
    }
    return ret;
}

ItemCapKindList Resolver::satifiedByInstalled( const PoolItem & item )
{
    ItemCapKindList ret;
    collectResolverInfo();

    for (ItemCapKindMap::const_iterator iter = _satifiedByInstalled.find(item); iter != _satifiedByInstalled.end();) {
        ItemCapKind info = iter->second;
        PoolItem iterItem = iter->first;
        if (iterItem == item) {
            ret.push_back(info);
            iter++;
        } else {
            // exit
            iter = _satifiedByInstalled.end();
        }
    }
    return ret;
}

ItemCapKindList Resolver::installedSatisfied( const PoolItem & item )
{
    ItemCapKindList ret;
    collectResolverInfo();

    for (ItemCapKindMap::const_iterator iter = _installedSatisfied.find(item); iter != _installedSatisfied.end();) {
        ItemCapKind info = iter->second;
        PoolItem iterItem = iter->first;
        if (iterItem == item) {
            ret.push_back(info);
            iter++;
        } else {
            // exit
            iter = _installedSatisfied.end();
        }
    }
    return ret;
}


///////////////////////////////////////////////////////////////////
    };// namespace detail
    /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////
  };// namespace solver
  ///////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////
};// namespace zypp
/////////////////////////////////////////////////////////////////////////

