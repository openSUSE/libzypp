/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/SolvIterMixin.cc
 *
*/
//#include <iostream>
//#include <zypp-core/base/Logger.h>

#include <zypp/sat/SolvIterMixin.h>
#include <zypp/sat/Solvable.h>
#include <zypp/ResPoolProxy.h>
#include <zypp/pool/PoolTraits.h>

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace sat
  { /////////////////////////////////////////////////////////////////

    namespace solvitermixin_detail
    {
      bool UnifyByIdent2::operator()( const Solvable & solv_r ) const
      {
        bool ret = false;
        if ( solv_r ) {
          // Need to use pool::ByIdent because packages and srcpackages have the same ident() id!
          sat::detail::IdType myident = pool::ByIdent( solv_r ).get();
          sat::detail::SolvableIdType myid = solv_r.id();
          auto inmapiter = _umap->insert( std::make_pair( myident, myid ) ).first;
          if ( inmapiter->second == myid )
            ret = true; // solv_r represents it's ident
        }
        return ret;
      }

#if LEGACY(1735)
      // It's flawed because the representing solvable (the 1st one that stored it's
      // ident id) can not be visited twice. Furthermore a UnifyByIdent instance is
      // shared among iterator copies. So no iterator is able to visit an item
      // that has been visited by itself or by another iterator copy before.
      bool UnifyByIdent::operator()( const Solvable & solv_r ) const
      {
        // Need to use pool::ByIdent because packages and srcpackages have the same id.
        return( solv_r && _uset->insert( pool::ByIdent( solv_r ).get() ).second );
      }
#endif
    }

    ///////////////////////////////////////////////////////////////////
    // asSolvable
    ///////////////////////////////////////////////////////////////////
    Solvable asSolvable::operator()( const PoolItem & pi_r ) const
    {
      return pi_r.satSolvable();
    }

    Solvable asSolvable::operator()( const ResObject_constPtr & res_r ) const
    {
      return res_r ? res_r->satSolvable() : Solvable();
    }

    /////////////////////////////////////////////////////////////////
  } // namespace sat
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  namespace ui
  { /////////////////////////////////////////////////////////////////

    Selectable_Ptr asSelectable::operator()( const sat::Solvable & sov_r ) const
    {
      return ResPool::instance().proxy().lookup( sov_r );
    }

    /////////////////////////////////////////////////////////////////
  } // namespace ui
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
