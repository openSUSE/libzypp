/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/zypp_detail/ZYppImpl.cc
 *
*/

#include <iostream>
#include <zypp/TmpPath.h>
#include <zypp/base/Logger.h>
#include <zypp/base/String.h>
#include <zypp/base/Env.h>

#include <zypp/zypp_detail/ZYppImpl.h>
#include <zypp/target/TargetImpl.h>
#include <zypp/ZYpp.h>
#include <zypp/DiskUsageCounter.h>
#include <zypp/ZConfig.h>
#include <zypp/sat/Pool.h>
#include <zypp/PoolItem.h>

#include <zypp/ZYppCallbacks.h>	// JobReport::instance
#include <zypp/zypp_detail/ZyppLock.h>

using std::endl;

#include <glib.h>
#include <zypp-core/zyppng/base/private/linuxhelpers_p.h>
#include <zypp-core/base/userrequestexception.h>

namespace {

  // Helper pipe to safely signal libzypp's poll code
  // to stop polling and throw a user abort exception.
  // Due to the pain that is unix signal handling we need
  // to use atomics and be very careful about what we are calling.
  //
  // Not cleaned up, this happens when the application that is using libzypp
  // exits.
  volatile sig_atomic_t shutdownPipeRead{-1};
  volatile sig_atomic_t shutdownPipeWrite{-1};

  bool makeShutdownPipe() {
    int pipeFds[]{ -1, -1 };
  #ifdef HAVE_PIPE2
      if ( ::pipe2( pipeFds, O_CLOEXEC ) != 0 )
        return false;
  #else
      if ( ::pipe( pipeFds ) != 0 )
        return false;
      ::fcntl( pipeFds[0], F_SETFD, O_CLOEXEC );
      ::fcntl( pipeFds[1], F_SETFD, O_CLOEXEC );
  #endif
      shutdownPipeRead = pipeFds[0];
      shutdownPipeWrite = pipeFds[1];
      return true;
  }

  const bool ensureShutdownPipe() {
    static auto pipesInitialized = makeShutdownPipe();
    return pipesInitialized;
  }

  const int shutdownPipeReadFd() {
    if ( !ensureShutdownPipe() )
      return -1;
    return static_cast<int>(shutdownPipeRead);
  }

  const int shutdownPipeWriteFd() {
    return static_cast<int>(shutdownPipeWrite);
  }

}

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////


  namespace env
  {
    /** Hack to circumvent the currently poor --root support.
     *  To be replaced by the sysRoot of the context the lock belongs to.
     */
    inline Pathname ZYPP_LOCKFILE_ROOT()
    { return getenv("ZYPP_LOCKFILE_ROOT") ? getenv("ZYPP_LOCKFILE_ROOT") : "/"; }
  }


  ///////////////////////////////////////////////////////////////////
  namespace media
  {
    ScopedDisableMediaChangeReport::ScopedDisableMediaChangeReport( bool condition_r )
    {
      static weak_ptr<callback::TempConnect<media::MediaChangeReport> > globalguard;
      if ( condition_r && ! (_guard = globalguard.lock()) )
      {
        // aquire a new one....
        _guard.reset( new callback::TempConnect<media::MediaChangeReport>() );
        globalguard = _guard;
      }
    }
  } // namespace media
  ///////////////////////////////////////////////////////////////////

  callback::SendReport<JobReport> & JobReport::instance()
  {
    static callback::SendReport<JobReport> _report;
    return _report;
  }


  ///////////////////////////////////////////////////////////////////
  namespace zypp_detail
  { /////////////////////////////////////////////////////////////////


    static bool zyppLegacyShutdownStarted = false; // set to true if the GlobalStateHelper was destructed

    // if this logic is changed, also update the one in MediaConfig
    zypp::Pathname autodetectZyppConfPath() {
      const char *env_confpath = getenv("ZYPP_CONF");
      return env_confpath ? env_confpath
                          : zyppng::ContextBase::defaultConfigPath();
    }

    zyppng::SyncContextRef &GlobalStateHelper::assertContext() {

      if ( zyppLegacyShutdownStarted )
        ZYPP_THROW("Global State requested after it was freed");

      // set up the global context for the legacy API
      // zypp is booting
      if (!_defaultContext) {
        auto set = zyppng::ContextSettings {
          .root = "/",
          .configPath = autodetectZyppConfPath ()
        };
        _defaultContext = zyppng::SyncContext::create();

        if ( !_config )
          config(); // assert config

        _defaultContext->legacyInit ( std::move(set),  _config );
      }
      return _defaultContext;
    }

    zyppng::FusionPoolRef<zyppng::SyncContext> &GlobalStateHelper::assertPool()
    {
      if ( !_defaultPool )
         _defaultPool = zyppng::FusionPool<zyppng::SyncContext>::defaultPool();
      return _defaultPool;
    }

    zypp::ZConfig &GlobalStateHelper::config() {

      if ( zyppLegacyShutdownStarted )
        ZYPP_THROW("Global State requested after it was freed");

      auto &inst = instance();
      if (!inst._config) {
        inst._config.reset( new ZConfig( autodetectZyppConfPath () ) );
      }

      return *inst._config;
    }

    void GlobalStateHelper::lock()
    {
      if ( zyppLegacyShutdownStarted )
        return;

      auto &me = instance ();
      if ( me._lock )
        return;

      const long LOCK_TIMEOUT = str::strtonum<long>( getenv( "ZYPP_LOCK_TIMEOUT" ) );
      auto l = ZyppContextLock::globalLock( env::ZYPP_LOCKFILE_ROOT () );
      l->acquireLock( LOCK_TIMEOUT );

      // did not throw, we got the lock
      me._lock = std::move(l);
    }

    void GlobalStateHelper::unlock()
    {
      if ( zyppLegacyShutdownStarted )
        return;

      auto &me = instance ();
      me._lock.reset();
    }

    Pathname GlobalStateHelper::lockfileDir()
    {
      return ZyppContextLock::globalLockfileDir( env::ZYPP_LOCKFILE_ROOT() ) ;
    }

    GlobalStateHelper::~GlobalStateHelper()
    {
      zyppLegacyShutdownStarted = true;
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : ZYppImpl::ZYppImpl
    //	METHOD TYPE : Constructor
    //
    ZYppImpl::ZYppImpl()
    {
      // trigger creation of the shutdown pipe
      if ( !ensureShutdownPipe() )
        WAR << "Failed to create shutdown pipe" << std::endl;

      GlobalStateHelper::config().about( MIL );
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : ZYppImpl::~ZYppImpl
    //	METHOD TYPE : Destructor
    //
    ZYppImpl::~ZYppImpl()
    {}

    //------------------------------------------------------------------------
    // add/remove resolvables

    DiskUsageCounter::MountPointSet ZYppImpl::diskUsage()
    {
      if ( ! _disk_usage )
      {
        setPartitions( DiskUsageCounter::detectMountPoints() );
      }
      return _disk_usage->disk_usage(pool());
    }

    void ZYppImpl::setPartitions(const DiskUsageCounter::MountPointSet &mp)
    {
      _disk_usage.reset(new DiskUsageCounter());
      _disk_usage->setMountPoints(mp);
    }

    DiskUsageCounter::MountPointSet ZYppImpl::getPartitions() const
    {
      if (_disk_usage)
        return _disk_usage->getMountPoints();
      else
        return DiskUsageCounter::detectMountPoints();
    }

    //------------------------------------------------------------------------
    // target

    Target_Ptr ZYppImpl::target() const
    {
      auto ref = getTarget();
      if ( !ref )
        ZYPP_THROW(Exception("Target not initialized."));
      return ref;
    }


    void ZYppImpl::initializeTarget( const Pathname & root, bool doRebuild_r )
    {
      context()->changeToTarget( root, doRebuild_r );
    }

    void ZYppImpl::finishTarget()
    {
      context()->finishTarget().unwrap();
    }

    //------------------------------------------------------------------------
    // commit

    /** \todo Remove workflow from target, lot's of it could be done here,
     * and target used for transact. */
    ZYppCommitResult ZYppImpl::commit( const ZYppCommitPolicy & policy_r )
    {
      return  GlobalStateHelper::pool()->commit( policy_r ).unwrap();
    }

    void ZYppImpl::installSrcPackage( const SrcPackage_constPtr & srcPackage_r )
    {
      if (! context()->target() )
        ZYPP_THROW( Exception("Target not initialized.") );
      context()->target()->_pimpl->installSrcPackage( srcPackage_r );
    }

    ManagedFile ZYppImpl::provideSrcPackage( const SrcPackage_constPtr & srcPackage_r )
    {
      if (! context()->target() )
        ZYPP_THROW( Exception("Target not initialized.") );
      return context()->target()->_pimpl->provideSrcPackage( srcPackage_r );
    }

    //------------------------------------------------------------------------
    // target store path

    Pathname ZYppImpl::homePath() const
    { return _home_path.empty() ? Pathname("/var/lib/zypp") : _home_path; }

    void ZYppImpl::setHomePath( const Pathname & path )
    { _home_path = path; }

    Pathname ZYppImpl::tmpPath() const
    { return zypp::myTmpDir(); }

    void ZYppImpl::setShutdownSignal()
    {
      // ONLY signal safe code here, that means no logging or anything else that is more than using atomics
      // or writing a fd
      int sigFd = shutdownPipeWriteFd();
      if ( sigFd == -1 ) {
        // we have no fd to set this
        return;
      }
      zyppng::eintrSafeCall( write, sigFd, "1", 1 );
    }

    void ZYppImpl::clearShutdownSignal()
    {
      // ONLY signal safe code here, that means no logging or anything else that is more than using atomics
      // or writing a fd
      int sigFd = shutdownPipeWriteFd();
      if ( sigFd == -1 ) {
        // we have no fd so nothing to clear
        return;
      }

      char buf = 0;
      while( zyppng::eintrSafeCall( read, sigFd, &buf, 1 ) > 0 )
        continue;
    }

    /******************************************************************
     **
     **	FUNCTION NAME : operator<<
     **	FUNCTION TYPE : std::ostream &
    */
    std::ostream & operator<<( std::ostream & str, const ZYppImpl & obj )
    {
      return str << "ZYppImpl";
    }

    int zypp_poll( std::vector<GPollFD> &fds, int timeout)
    {
      // request a shutdown fd we can use
      const auto shutdownFd = shutdownPipeReadFd();
      if (shutdownFd == -1) {
        ZYPP_THROW( zypp::Exception("Failed to get shutdown pipe") );
      }

      fds.push_back( GPollFD {
        .fd       = shutdownFd,
        .events   = G_IO_IN,
        .revents  = 0
      });

      // make sure to remove our fd again
      OnScopeExit removeShutdownFd( [&](){ fds.pop_back(); } );

      int r = zyppng::eintrSafeCall( g_poll, fds.data(), fds.size(), timeout );
      if ( r > 0 ) {
        // at least one fd triggered, if its our's we throw
        if ( fds.back().revents )
          ZYPP_THROW( UserRequestException( UserRequestException::ABORT, "Shutdown signal received during poll." ) );
      }
      return r;
    }
  } // namespace zypp_detail
  ///////////////////////////////////////////////////////////////////

  Pathname myTmpDir() // from TmpPath.h
  {
      static filesystem::TmpDir _tmpdir(filesystem::TmpPath::defaultLocation(),
                                        "zypp.");
      return _tmpdir.path();
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
