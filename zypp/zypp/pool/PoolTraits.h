/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/pool/PoolTraits.h
 *
*/
#ifndef ZYPP_POOL_POOLTRAITS_H
#define ZYPP_POOL_POOLTRAITS_H

#include <set>
#include <map>
#include <list>
#include <vector>

#include <zypp/base/Iterator.h>
#include <zypp/base/Hash.h>

#include <zypp/PoolItem.h>
#include <zypp/pool/ByIdent.h>
#include <zypp/sat/Pool.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  class PoolQuery;

  ///////////////////////////////////////////////////////////////////
  namespace pool
  { /////////////////////////////////////////////////////////////////

    class PoolImpl;

    /** Pool internal filter skiping invalid/unwanted PoolItems. */
    struct ByPoolItem
    {
      bool operator()( const PoolItem & pi ) const
      { return bool(pi); }
    };

    /** std::_Select2nd
     */
    template<typename TPair>
    struct P_Select2nd
    {
      typename TPair::second_type&
      operator()(TPair& __x) const
      { return __x.second; }

      const typename TPair::second_type&
      operator()(const TPair& __x) const
      { return __x.second; }
    };

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : PoolTraits
    //
    /** */
    struct PoolTraits
    {
    public:
      using SolvableIdType = sat::detail::SolvableIdType;

      /** pure items  */
      using ItemContainerT = std::vector<PoolItem>;
      using item_iterator = ItemContainerT::const_iterator;
      using const_iterator = filter_iterator<ByPoolItem, ItemContainerT::const_iterator>;
      using size_type = ItemContainerT::size_type;

      /** ident index */
      using Id2ItemT = std::unordered_multimap<sat::detail::IdType, PoolItem>;
      using Id2ItemValueSelector = P_Select2nd<Id2ItemT::value_type>;
      using byIdent_iterator = transform_iterator<Id2ItemValueSelector, Id2ItemT::const_iterator>;

      /** list of known Repositories */
      using repository_iterator = sat::Pool::RepositoryIterator;

      /** hard locks from etc/zypp/locks */
      using HardLockQueries = std::list<PoolQuery>;
      using hardLockQueries_iterator = HardLockQueries::const_iterator;

      using Impl = PoolImpl;
      using Impl_Ptr = shared_ptr<PoolImpl>;
      using Impl_constPtr = shared_ptr<const PoolImpl>;
    };
    ///////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////
  } // namespace pool
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_POOL_POOLTRAITS_H
