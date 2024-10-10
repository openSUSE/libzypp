/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ZYpp.h
 *
*/
#ifndef ZYPP_ZYPP_H
#define ZYPP_ZYPP_H
#include <iosfwd>
#include <memory>

#include <zypp/base/NonCopyable.h>
#include <zypp/base/PtrTypes.h>
#include <zypp-core/Globals.h>

#include <zypp/ZConfig.h>
#include <zypp/ManagedFile.h>

#include <zypp/ZYppCommit.h>
#include <zypp/ResTraits.h>

#include <zypp/Target.h>
#include <zypp/Resolver.h>
#include <zypp/KeyRing.h>
#include <zypp/DiskUsageCounter.h>

namespace zyppng {
  class Context;
}

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  namespace zypp_detail
  {
    class ZYppImpl;
  }

  class ZYppFactory;
  class ResPool;
  class ResPoolProxy;
  class KeyRing;

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : ZYpp
  //
  /**
   * \todo define Exceptions
   * ZYpp API main interface
   */
  class ZYPP_API ZYpp : private base::NonCopyable
  {
    friend std::ostream & operator<<( std::ostream & str, const ZYpp & obj );

  public:
    using Ptr = std::shared_ptr<ZYpp>;
    using constPtr = std::shared_ptr<const ZYpp>;

  public:

    /**
     * Access to the global resolvable pool.
     * Same as \ref zypp::ResPool::instance
     */
    ResPool pool() const;

    /** Pool of ui::Selectable.
     * Based on the ResPool, ui::Selectable groups ResObjetcs of
     * same kind and name.
    */
    ResPoolProxy poolProxy() const;

    DiskUsageCounter::MountPointSet diskUsage();

    void setPartitions(const DiskUsageCounter::MountPointSet &mp);

    DiskUsageCounter::MountPointSet getPartitions() const;

  public:
    /**
     * \throws Exception
     */
    Target_Ptr target() const;

    /** Same as \ref target but returns NULL if target is not
     *  initialized, instead of throwing.
     */
    Target_Ptr getTarget() const;

    /**
     * \throws Exception
     * Just init the target, dont populate store or pool.
     * If \c doRebuild_r is \c true, an already existing
     * database is rebuilt (rpm --rebuilddb ).
     */
    void initializeTarget(const Pathname & root, bool doRebuild_r = false);

    /**
     * \throws Exception
     */
    void finishTarget();

    /**
     * To be called from zyppers signal handlers. Sets a shutdown signal
     * in a signal and thread safe way.
     */
    static void setShutdownSignal();

    /**
     * To be called from zypper code. This resets the shutdown signal state
     * to normal..
     */
    static void clearShutdownSignal();

  public:
    using CommitResult = ZYppCommitResult;

    /** Commit changes and transactions.
     * \param \ref CommitPolicy
     * \return \ref CommitResult
     * \throws Exception
    */
    ZYppCommitResult commit( const ZYppCommitPolicy & policy_r );

    /** Install a source package on the Target.
     * \throws Exception
     */
    void installSrcPackage( const SrcPackage_constPtr & srcPackage_r );

    /** Provides a source package on the Target.
     * \throws Exception
     */
    ManagedFile provideSrcPackage( const SrcPackage_constPtr & srcPackage_r );

  public:
    /** */
    Resolver_Ptr resolver() const;
    KeyRing_Ptr keyRing() const;

  public:
    /** Get the path where zypp related plugins store persistent data and caches   */
    Pathname homePath() const;

    /** Get the path where zypp related plugins store temp data   */
    Pathname tmpPath() const;

    /** set the home, if you need to change it */
    void setHomePath( const Pathname & path );

  private:
    /** Factory */
    friend class ZYppFactory;
    using Impl = zypp_detail::ZYppImpl;
    using Impl_Ptr = std::shared_ptr<Impl>;
    /** Factory ctor */
    explicit ZYpp( const Impl_Ptr & impl_r );
  private:
    /** Deleted via shared_ptr */
    friend void ::boost::checked_delete<ZYpp>(ZYpp*) BOOST_NOEXCEPT;	// template<class T> inline void checked_delete(T * x)
  public:
    /** Dtor */
    ~ZYpp();
  private:
    /** Pointer to implementation */
    RW_pointer<Impl> _pimpl;
  };
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_ZYPP_H
