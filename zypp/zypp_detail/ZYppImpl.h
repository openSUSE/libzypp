/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/zypp_detail/ZYppImpl.h
 *
*/
#ifndef ZYPP_ZYPP_DETAIL_ZYPPIMPL_H
#define ZYPP_ZYPP_DETAIL_ZYPPIMPL_H

#include <iosfwd>

#include <zypp/TmpPath.h>
#include <zypp/Target.h>
#include <zypp/Resolver.h>
#include <zypp/KeyRing.h>
#include <zypp/ZYppCommit.h>
#include <zypp/ResTraits.h>
#include <zypp/DiskUsageCounter.h>
#include <zypp/ManagedFile.h>

#include <zypp/ng/Context>
#include <zypp/ng/fusionpool.h>

using GPollFD = struct _GPollFD;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace zypp_detail
  { /////////////////////////////////////////////////////////////////

    // Legacy detect zypp conf path
    zypp::Pathname autodetectZyppConfPath();

    // represents the legacy zypp global state, will initialize the internal
    // context so that the required parts are initialized as needed
    class GlobalStateHelper {

    public:
      static zypp::ZConfig &config();

      static zyppng::SyncContextRef context() {
        return instance().assertContext();
      }

      static zypp::Target_Ptr target() {
        return instance().assertContext()->target();
      }

      static zyppng::FusionPoolRef<zyppng::SyncContext> pool() {
        return instance().assertPool();
      }

      static void lock();
      static void unlock();

      static zypp::Pathname lockfileDir();

    private:
      GlobalStateHelper() {}
      ~GlobalStateHelper();

      static GlobalStateHelper &instance() {
        static GlobalStateHelper me;
        return me;
      }

      ZyppContextLockRef _lock;

      zyppng::SyncContextRef &assertContext();
      zyppng::FusionPoolRef<zyppng::SyncContext> &assertPool();

      zyppng::SyncContextRef _defaultContext;
      zyppng::FusionPoolRef<zyppng::SyncContext> _defaultPool;

      ZConfigRef _config; // legacy has one config that never goes away
    };


    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : ZYppImpl
    //
    /** */
    class ZYppImpl
    {
      friend std::ostream & operator<<( std::ostream & str, const ZYppImpl & obj );

    public:
      /** Default ctor */
      ZYppImpl();
      /** Dtor */
      ~ZYppImpl();

    public:

      zyppng::SyncContextRef context() {
        return GlobalStateHelper::context();
      }

      /** */
      ResPool pool() const
      { return GlobalStateHelper::pool()->resPool(); }

      ResPoolProxy poolProxy() const
      { return GlobalStateHelper::pool()->poolProxy(); }

      /** */
      KeyRing_Ptr keyRing() const
      { return GlobalStateHelper::context()->keyRing(); }


      Resolver_Ptr resolver() const
      { return GlobalStateHelper::pool()->resolver(); }

    public:
      /** \todo Signal locale change. */
      /**
       * \throws Exception
       */
      Target_Ptr target() const;

      /** Same as \ref target but returns NULL if target is not
       *  initialized, instead of throwing.
       */
      Target_Ptr getTarget() const
      { return GlobalStateHelper::target(); }

      /**
       * \throws Exception
       * true, just init the target, dont populate store or pool
       */
      void initializeTarget( const Pathname & root, bool doRebuild_r );

      /**
       * \throws Exception
       */
      void finishTarget();

      /** Commit changes and transactions. */
      ZYppCommitResult commit( const ZYppCommitPolicy & policy_r );

      /** Install a source package on the Target. */
      void installSrcPackage( const SrcPackage_constPtr & srcPackage_r );

      /** Install a source package on the Target. */
      ManagedFile provideSrcPackage( const SrcPackage_constPtr & srcPackage_r );

    public:
      /** Get the path where zypp related plugins store persistent data and caches   */
      Pathname homePath() const;

      /** Get the path where zypp related plugins store tmp data   */
      Pathname tmpPath() const;

      /** set the home, if you need to change it */
      void setHomePath( const Pathname & path );

    public:
      DiskUsageCounter::MountPointSet diskUsage();
      void setPartitions(const DiskUsageCounter::MountPointSet &mp);
      DiskUsageCounter::MountPointSet getPartitions() const;

    public:
      /** Hook for actions to trigger if the Target changes (initialize/finish) */
      void changeTargetTo( const Target_Ptr& newtarget_r );

      /**
       * Enable the shutdown signal for \ref zypp_poll calls
       */
      static void setShutdownSignal();

      /**
       * Disable the shutdown signal for \ref zypp_poll calls
       */
      static void clearShutdownSignal();

    private:
      /** */
      Pathname _home_path;
      /** defined mount points, used for disk usage counting */
      shared_ptr<DiskUsageCounter> _disk_usage;
    };
    ///////////////////////////////////////////////////////////////////

    /** \relates ZYppImpl Stream output */
    std::ostream & operator<<( std::ostream & str, const ZYppImpl & obj );

    /**
     * Small wrapper around g_poll that additionally listens to the shutdown FD
     * returned by \ref ZYpp::shutdownSignalFd.
     *
     * EINTR is handled internally, no need to explicitely handle it.
     *
     * For zyppng related code we should use different means to cancel processes,
     * e.g via the top level event loop
     *
     * \throws zypp::UserAbortException in case the shutdown signal was received
     */
    int zypp_poll( std::vector<GPollFD> &fds, int timeout = -1 );
    /////////////////////////////////////////////////////////////////
  } // namespace zypp_detail
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_ZYPP_DETAIL_ZYPPIMPL_H
