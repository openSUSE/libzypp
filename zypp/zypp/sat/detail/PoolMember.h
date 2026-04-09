/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/detail/PoolMember.h
 *
*/
#ifndef ZYPP_SAT_DETAIL_POOLMEMBER_H
#define ZYPP_SAT_DETAIL_POOLMEMBER_H

#include <zypp-core/base/Hash.h>
#include <zypp-core/base/Iterator.h>
#include <zypp-core/base/String.h>
#include <zypp-core/base/Easy.h>

#include <zypp/sat/detail/PoolDefines.h>

namespace zypp
{
  class IdString;
  class Capability;
  class Capabilities;
  class Repository;
  class RepoInfo;

  namespace detail
  {
    class RepoIterator;
    struct ByRepository;
  }

  namespace sat
  {
    class Pool;
    class Solvable;

    namespace detail
    {
      class PoolImpl;
      //
      //	CLASS NAME : PoolMember
      //
      /** Backlink to the associated \ref PoolImpl.
       * Simple as we currently use one global PoolImpl. If we change our
       * minds this is where we'd store and do the \c Id to \ref PoolImpl
       * mapping.
       */
      struct PoolMember
      {
        static PoolImpl & myPool();
        static bool poolValid();
      };
    } // namespace detail

    namespace detail
    {
      class SolvableIterator;
    } // namespace detail
  } // namespace sat
} // namespace zypp
#endif // ZYPP_SAT_DETAIL_POOLMEMBER_H
