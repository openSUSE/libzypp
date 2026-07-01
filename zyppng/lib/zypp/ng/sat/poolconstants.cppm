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
module;
#include <zypp/ng/sat/solvincludes.h>
#include <zypp/sat/detail/PoolDefines.h>
#include <zypp/ng/zypp_test_export.h>

export module zyppng:sat_poolconstants;

// Module-purview-only block — reachable across the zyppng module's partitions
// (incl. test_access) but NOT exported to consumers of `import zyppng;`.
ZYPPNG_TEST_EXPORT namespace zyppng::sat::detail {
    using CDataiterator = zypp::sat::detail::CDataiterator;
    using CDatamatcher  = zypp::sat::detail::CDatamatcher;
    using CMap          = zypp::sat::detail::CMap;
    using CPool         = zypp::sat::detail::CPool;
    using CQueue        = zypp::sat::detail::CQueue;
    using CRepo         = zypp::sat::detail::CRepo;
    using CSolvable     = zypp::sat::detail::CSolvable;
    using CSolver       = zypp::sat::detail::CSolver;
    using CTransaction  = zypp::sat::detail::CTransaction;

    using IdType         = zypp::sat::detail::IdType;
    using SolvableIdType = zypp::sat::detail::SolvableIdType;
    using RepoIdType     = zypp::sat::detail::RepoIdType;

    // PoolDefines.h constants are declared with ZYPP_DEFINE_GLOBAL_CONSTANT which
    // expands to 'inline constexpr' under ZYPPNG.  However GMF-included entities
    // are not reachable by importers even with external linkage — 'export using'
    // of a GMF name is ill-formed.  We therefore redeclare them here as
    // 'inline constexpr' in the module purview using the canonical libsolv macro
    // values.  The static_asserts in pool.cc guard value parity at compile time.
    inline constexpr IdType         noId                 { STRID_NULL           };
    inline constexpr IdType         emptyId              { STRID_EMPTY          };
    inline constexpr IdType         solvablePrereqMarker { SOLVABLE_PREREQMARKER };
    inline constexpr IdType         solvableFileMarker   { SOLVABLE_FILEMARKER  };
    inline constexpr IdType         namespaceModalias    { NAMESPACE_MODALIAS   };
    inline constexpr IdType         namespaceLanguage    { NAMESPACE_LANGUAGE   };
    inline constexpr IdType         namespaceFilesystem  { NAMESPACE_FILESYSTEM };
    inline constexpr SolvableIdType noSolvableId         { ID_NULL              };
    inline constexpr SolvableIdType systemSolvableId     { SYSTEMSOLVABLE       };
    inline constexpr RepoIdType     noRepoId             { nullptr              };
    // isDepMarkerId is 'inline bool' in PoolDefines.h — it IS reachable via using
    // because inline functions in the GMF have external linkage and their bodies
    // are emitted into the BMI.
    using zypp::sat::detail::isDepMarkerId;

    using size_type = SolvableIdType;
  } // namespace zyppng::sat::detail

export namespace zyppng::sat {

  /**
   * \brief Defines the scope of an invalidation request for the Pool.
   * \details This allows providers to notify the pool of changes, avoiding
   * unnecessary cache rebuilding.
   */
  enum class PoolInvalidation {
    /** Only external requirements/context changed (e.g., Locales).
     *  The pool content is still valid, but whatprovides must be updated. */
    Dependency,

    /** Structural or data change (e.g., Rootfs, Arch, or Repo content). */
    Data
  };

} // namespace zyppng::sat
