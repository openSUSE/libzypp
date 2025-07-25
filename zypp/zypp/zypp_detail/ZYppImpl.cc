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
#include <zypp-core/base/Env.h>

#include <zypp/zypp_detail/ZYppImpl.h>
#include <zypp/target/TargetImpl.h>
#include <zypp/ZYpp.h>
#include <zypp/DiskUsageCounter.h>
#include <zypp/ZConfig.h>
#include <zypp/sat/Pool.h>
#include <zypp/PoolItem.h>

#include <zypp/ZYppCallbacks.h>	// JobReport::instance

using std::endl;

#include <glib.h>
#include <zypp-core/ng/base/private/linuxhelpers_p.h>
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

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : ZYppImpl::ZYppImpl
    //	METHOD TYPE : Constructor
    //
    ZYppImpl::ZYppImpl()
    : _target( nullptr )
    , _resolver( new Resolver( ResPool::instance()) )
    {
      // trigger creation of the shutdown pipe
      if ( !ensureShutdownPipe() )
        WAR << "Failed to create shutdown pipe" << std::endl;

      ZConfig::instance().about( MIL );
      MIL << "Initializing keyring..." << std::endl;
      _keyring = new KeyRing(tmpPath());
      _keyring->allowPreload( true );
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
      if (! _target)
        ZYPP_THROW(Exception("Target not initialized."));
      return _target;
    }

    void ZYppImpl::changeTargetTo( const Target_Ptr& newtarget_r )
    {
      if ( _target && newtarget_r ) // bsc#1203760: Make sure the old target is deleted before a new one is created!
        INT << "2 active targets at the same time must not happen!" << endl;
      _target = newtarget_r;
      ZConfig::instance().notifyTargetChanged();
      resolver()->setDefaultSolverFlags( /*all_r*/false );  // just changed defaults
    }

    void ZYppImpl::initializeTarget( const Pathname & root, bool doRebuild_r )
    {
      MIL << "initTarget( " << root << (doRebuild_r?", rebuilddb":"") << ")" << endl;
      if (_target) {
          if (_target->root() == root) {
              MIL << "Repeated call to initializeTarget()" << endl;
              return;
          }
          _target->unload();
          _target = nullptr;  // bsc#1203760: Make sure the old target is deleted before a new one is created!
      }
      changeTargetTo( new Target( root, doRebuild_r ) );
      _target->buildCache();
    }

    void ZYppImpl::finishTarget()
    {
      if (_target)
          _target->unload();

      changeTargetTo( nullptr );
    }

    //------------------------------------------------------------------------
    // commit

    /** \todo Remove workflow from target, lot's of it could be done here,
     * and target used for transact. */
    ZYppCommitResult ZYppImpl::commit( const ZYppCommitPolicy & policy_r )
    {
      if ( getenv("ZYPP_TESTSUITE_FAKE_ARCH") )
      {
        ZYPP_THROW( Exception("ZYPP_TESTSUITE_FAKE_ARCH set. Commit not allowed and disabled.") );
      }

      MIL << "Attempt to commit (" << policy_r << ")" << endl;
      if (! _target)
        ZYPP_THROW( Exception("Target not initialized.") );


      env::ScopedSet ea { "ZYPP_IS_RUNNING", str::numstring(getpid()).c_str() };
      env::ScopedSet eb;
      if ( _target->chrooted() )
        eb = env::ScopedSet( "SYSTEMD_OFFLINE", "1" );	// bsc#1118758 - indicate no systemd if chrooted install
      env::ScopedSet ec;
      {
        std::string val;
        if ( const char * env = getenv("GLIBC_TUNABLES"); env ) { // colon-separated list of k=v pairs
          val = env;
          val += ":";
        }
        val += "glibc.cpu.hwcap_mask=0";   // bsc#1246912 - make ld.so ignore the subarch packages
        ec = env::ScopedSet( "GLIBC_TUNABLES", val.c_str() );
      }

      ZYppCommitResult res = _target->_pimpl->commit( pool(), policy_r );

      if (! policy_r.dryRun() )
      {
        if ( policy_r.syncPoolAfterCommit() )
          {
            // reload new status from target
            DBG << "reloading " << sat::Pool::instance().systemRepoAlias() << " repo to pool" << endl;
            _target->load();
          }
        else
          {
            DBG << "unloading " << sat::Pool::instance().systemRepoAlias() << " repo from pool" << endl;
            _target->unload();
          }
      }

      MIL << "Commit (" << policy_r << ") returned: "
          << res << endl;
      return res;
    }

    void ZYppImpl::installSrcPackage( const SrcPackage_constPtr & srcPackage_r )
    {
      if (! _target)
        ZYPP_THROW( Exception("Target not initialized.") );
      _target->_pimpl->installSrcPackage( srcPackage_r );
    }

    ManagedFile ZYppImpl::provideSrcPackage( const SrcPackage_constPtr & srcPackage_r )
    {
      if (! _target)
        ZYPP_THROW( Exception("Target not initialized.") );
      return _target->_pimpl->provideSrcPackage( srcPackage_r );
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

    /////////////////////////////////////////////////////////////////
  } // namespace zypp_detail
  ///////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
