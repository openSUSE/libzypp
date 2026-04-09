/*---------------------------------------------------------------------\
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
#ifndef ZYPP_NG_SAT_POOLDEFINES_H_INCLUDED
#define ZYPP_NG_SAT_POOLDEFINES_H_INCLUDED


extern "C"
{
#include <solv/pool.h>
#include <solv/repo.h>
#include <solv/solvable.h>
#include <solv/poolarch.h>
#include <solv/repo_solv.h>
#include <solv/pool_parserpmrichdep.h>
}

#include <zypp/sat/detail/PoolDefines.h>

namespace zyppng::sat {

  namespace detail {
    using CDataiterator = zypp::sat::detail::CDataiterator;
    using CDatamatcher = zypp::sat::detail::CDatamatcher;
    using CMap = zypp::sat::detail::CMap;
    using CPool = zypp::sat::detail::CPool;
    using CQueue = zypp::sat::detail::CQueue;
    using CRepo = zypp::sat::detail::CRepo;
    using CSolvable = zypp::sat::detail::CSolvable;
    using CSolver = zypp::sat::detail::CSolver;
    using CTransaction = zypp::sat::detail::CTransaction;

    using IdType = zypp::sat::detail::IdType;
    using SolvableIdType = zypp::sat::detail::SolvableIdType;
    using RepoIdType = zypp::sat::detail::RepoIdType;

    using zypp::sat::detail::noId;
    using zypp::sat::detail::emptyId;
    using zypp::sat::detail::solvablePrereqMarker;
    using zypp::sat::detail::solvableFileMarker;
    using zypp::sat::detail::namespaceModalias;
    using zypp::sat::detail::namespaceLanguage;
    using zypp::sat::detail::namespaceFilesystem;
    using zypp::sat::detail::noSolvableId;
    using zypp::sat::detail::systemSolvableId;
    using zypp::sat::detail::noRepoId;
    using zypp::sat::detail::isDepMarkerId;

    using size_type = SolvableIdType;
  }

  /**
   * \brief Defines the scope of an invalidation request for the Pool.
   * \details This allows providers to notify the pool of changes, avoiding unnecessary cache rebuilding.
   */
  enum class PoolInvalidation {
    /**
     * \brief Only external requirements/context changed (e.g., Locales).
     * \details The pool content is still valid, but whatprovides must be updated
     */
    Dependency,

    /**
     * \brief Structural or data change (e.g., Rootfs, Arch, or Repo content).
     */
    Data
  };


}

#endif //ZYPP_NG_SAT_POOLDEFINES_H_INCLUDED
