/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* SolutionAction.cc
 *
 * Easy-to use interface to the ZYPP dependency resolver
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

#define ZYPP_USE_RESOLVER_INTERNALS

#include <zypp/solver/detail/Resolver.h>
#include <zypp/solver/detail/SolutionAction.h>
#include <zypp/solver/detail/SolverQueueItem.h>
#include <zypp/Capabilities.h>
#include <zypp-core/base/Logger.h>

using std::endl;

/////////////////////////////////////////////////////////////////////////
namespace zypp::solver::detail {

  ///////////////////////////////////////////////////////////////////////
  // class SolutionAction
  ///////////////////////////////////////////////////////////////////////

  IMPL_PTR_TYPE(SolutionAction);

  SolutionAction::SolutionAction()
  {}

  SolutionAction::~SolutionAction()
  {}

  std::ostream & SolutionAction::dumpOn( std::ostream & os ) const
  { return os << "SolutionAction<not specified> "; }

  PoolItem SolutionAction::item() const
  { return PoolItem(); }

  bool SolutionAction::skipsPatchesOnly() const
  { return false; }

  std::ostream & operator<<( std::ostream & str, const SolutionActionList & actionlist )
  {
    for ( const auto & itemptr : actionlist )
      str << *itemptr << std::endl;
    return str;
  }

  ///////////////////////////////////////////////////////////////////////
  // class TransactionSolutionAction
  ///////////////////////////////////////////////////////////////////////

  std::ostream & TransactionSolutionAction::dumpOn( std::ostream & str ) const
  {
    str << "TransactionSolutionAction: ";
    switch (_action) {
      case KEEP:				str << "Keep " << _item; break;
      case INSTALL:			str << "Install " << _item; break;
      case REMOVE:			str << "Remove " << _item; break;
      case UNLOCK:			str << "Unlock " << _item; break;
      case LOCK:				str << "Lock " << _item; break;
      case REMOVE_EXTRA_REQUIRE:		str << "Remove require " << _capability; break;
      case REMOVE_EXTRA_CONFLICT:		str << "Remove conflict " << _capability; break;
      case ADD_SOLVE_QUEUE_ITEM:		str << "Add SolveQueueItem " <<  _solverQueueItem; break;
      case REMOVE_SOLVE_QUEUE_ITEM:	str << "Remove SolveQueueItem " <<  _solverQueueItem; break;
    }
    return str;
  }

  bool TransactionSolutionAction::execute( ResolverInternal & resolver ) const
  {
    bool ret = true;
    switch ( _action ) {
      case KEEP:
        _item.status().resetTransact( ResStatus::USER );
        ret = _item.status().setTransact( false, ResStatus::APPL_HIGH ); // APPL_HIGH: Locking should not be saved permanently
        break;
      case INSTALL:
        if ( _item.status().isToBeUninstalled() )
          ret = _item.status().setTransact( false, ResStatus::USER );
        else
          _item.status().setToBeInstalled( ResStatus::USER );
        break;
      case REMOVE:
        if ( _item.status().isToBeInstalled() ) {
          _item.status().setTransact( false, ResStatus::USER );
          _item.status().setLock( true, ResStatus::USER ); // no other dependency can set it again
        } else if ( _item.status().isInstalled() )
          _item.status().setToBeUninstalled( ResStatus::USER );
        else
          _item.status().setLock( true,ResStatus::USER ); // no other dependency can set it again
        break;
      case UNLOCK:
        ret = _item.status().setLock( false, ResStatus::USER );
        if ( !ret ) ERR << "Cannot unlock " << _item << endl;
        break;
      case LOCK:
        _item.status().resetTransact( ResStatus::USER );
        ret = _item.status().setLock( true, ResStatus::APPL_HIGH ); // APPL_HIGH: Locking should not be saved permanently
        if ( !ret ) ERR << "Cannot lock " << _item << endl;
        break;
      case REMOVE_EXTRA_REQUIRE:
        resolver.removeExtraRequire( _capability );
        break;
      case REMOVE_EXTRA_CONFLICT:
        resolver.removeExtraConflict( _capability );
        break;
      case ADD_SOLVE_QUEUE_ITEM:
        resolver.addQueueItem( _solverQueueItem );
        break;
      case REMOVE_SOLVE_QUEUE_ITEM:
        resolver.removeQueueItem( _solverQueueItem );
        break;
      default:
        ERR << "Wrong TransactionKind" << endl;
        ret = false;
        break;
    }
    return ret;
  }

  bool TransactionSolutionAction::skipsPatchesOnly() const
  { return _action == KEEP && _item.isKind<Patch>(); }

  ///////////////////////////////////////////////////////////////////////
  // class InjectSolutionAction
  ///////////////////////////////////////////////////////////////////////

  std::ostream & InjectSolutionAction::dumpOn( std::ostream & str ) const
  {
    str << "InjectSolutionAction: ";
    switch (_kind) {
        case WEAK:	str << "Weak"; break;
        default:	str << "Wrong kind"; break;
    }
    return str << " " << _item;
  }

  bool InjectSolutionAction::execute( ResolverInternal & resolver ) const
  {
    bool ret = true;
    switch (_kind) {
      case WEAK:
        // set item dependencies to weak
        resolver.addWeak( _item );
        break;
      default:
        ERR << "No valid InjectSolutionAction kind found" << endl;
        ret = false;
        break;
    }
    return ret;
  }

} // namespace zypp::solver::detail
/////////////////////////////////////////////////////////////////////////
