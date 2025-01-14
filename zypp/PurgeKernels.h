/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/PurgeKernels.h
 *
*/

#include <iosfwd>
#include <utility>
#include <map>
#include <set>
#include <unordered_set>

#include <zypp/Globals.h>
#include <zypp/PoolItem.h>
#include <zypp/base/PtrTypes.h>

namespace zypp {

  namespace str {
  class regex;
  }

  /*!
   * Implements the logic of the "purge-kernels" command.
   *
   */
  class ZYPP_API PurgeKernels
  {
  public:
    PurgeKernels();


    /*!
     * Marks all currently obsolete Kernels according to the keep spec.
     * \note This will not commit the changes
     */
    void markObsoleteKernels();

    /*!
     * Force a specific uname to be set, only used for testing,
     * in production the running kernel is detected.
     */
    void setUnameR( const std::string &val );
    std::string unameR() const;


    /*!
     * Force a specific kernel arch to be set, only used for testing,
     * in production the running kernel arch is detected.
     */
    void setKernelArch( const zypp::Arch &arch );
    Arch kernelArch() const;

    /*!
       * Overrides the keep spec, the default value is read from ZConfig.
       * The keep spec is a string of tokens seperated by ",".
       * It only supports 3 different tokens:
       *  - "running" matches only the currently running kernel of the system
       *  - "oldest"  matches the kernel version for each flavour/arch combination with the lowest edition
       *              can be modified with a positive number:  oldest+n
       *  - "latest"  matches the kernel version for each flavour/arch combination with the highest edition
       *              can be modified with a negative number:  latest-n
       */
    void setKeepSpec( const std::string &val );
    std::string keepSpec () const;

  public:
    /// Helper for version sorted solvable container
    struct byNVRA {
      bool operator()( const sat::Solvable & lhs, const sat::Solvable & rhs ) const
      { return compareByNVRA( lhs, rhs ) < 0; }
    };

    using Bucket     = std::set<sat::Solvable,byNVRA>;              ///< fitting KMP versions
    using KmpBuckets = std::map<IdString,Bucket>;                   ///< KMP name : fitting KMP versions
    using KernelKmps = std::map<sat::Solvable,KmpBuckets,byNVRA>;   ///< kernel : KMP name : fitting KMP versions

    using PurgableKmps = std::unordered_set<sat::Solvable>;         ///< KMPs found to be superfluous

    /**
     * Suggest purgable KMP versions.
     *
     * Per kernel whose \ref ResStatus::staysInstalled return the installed KMPs whose requirements
     * are satisfied by this kernel. Intended to be run after \ref markObsoleteKernels. It returns
     * the KMP stats for the kernels kept and suggests the KMPs which could be purged because they may
     * be superfluous. It does not change the ResPool status.
     *
     * \note The computation here is solely based on kernel<>KMP requirements. Requirements between
     * a KMP and other KMPs or packages are not taken into accout here. So you should \b never file
     * hard remove requests for PurgableKmps. Use weak requests, so they stay in case other non-kernel
     * packages need them.
     *
     * For each kernel just the highest KMP version in it's Buckets is needed. Lower versions
     * may be needed by different kernels or may be superfluous.
     *
     * \note A special entry for \ref noSolvable may exist in \ref KernelKmps. It collects all
     * KMPs which do not have any requirements regarding a (kept) kernel at all. Whether they are
     * needed or are superfluous can not be decided here. We leave leave them untouched. Maybe they
     * are removed along with a purged kernel.
     *
     * The optional \a checkSystem_r argument is for testing and debugging only. If set to \c false
     * the stats are built for all kernel and KMP versions available in the repositories.
     *
     * \code
     * ==== (39558)kernel-default-5.14.21-150400.24.136.1.x86_64(@System)
     *      cluster-md-kmp-default fit 1 versions(s)
     *          * (39429)cluster-md-kmp-default-5.14.21-150400.24.136.1.x86_64(@System)
     *      crash-kmp-default fit 1 versions(s)
     *          * (39441)crash-kmp-default-7.3.0_k5.14.21_150400.24.49-150400.3.5.8.x86_64(@System)
     *      dlm-kmp-default fit 1 versions(s)
     *          * (39467)dlm-kmp-default-5.14.21-150400.24.136.1.x86_64(@System)
     *      drbd-kmp-default fit 3 versions(s)
     *          - (39477)drbd-kmp-default-9.0.30~1+git.10bee2d5_k5.14.21_150400.22-150400.1.75.x86_64(@System)
     *          - (39478)drbd-kmp-default-9.0.30~1+git.10bee2d5_k5.14.21_150400.24.11-150400.3.2.9.x86_64(@System)
     *          * (39476)drbd-kmp-default-9.0.30~1+git.10bee2d5_k5.14.21_150400.24.46-150400.3.4.1.x86_64(@System)
     *      gfs2-kmp-default fit 1 versions(s)
     *          * (39501)gfs2-kmp-default-5.14.21-150400.24.136.1.x86_64(@System)
     *      ocfs2-kmp-default fit 1 versions(s)
     *          * (39948)ocfs2-kmp-default-5.14.21-150400.24.136.1.x86_64(@System)
     * \endcode
     */
    static std::pair<KernelKmps,PurgableKmps> clusterKmps( const bool checkSystem_r = true );

    struct Impl;
  private:
    RW_pointer<Impl> _pimpl;
  };

  /** \relates PurgeKernels::KernelKmps stream output */
  std::ostream & operator<<( std::ostream & str, const PurgeKernels::KernelKmps & obj );
}

