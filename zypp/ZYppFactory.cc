/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ZYppFactory.cc
 *
*/
extern "C"
{
#include <sys/file.h>
}
#include <iostream>
#include <fstream>
#include <signal.h>

#include <zypp/base/Logger.h>
#include <zypp/base/LogControl.h>
#include <zypp/base/Gettext.h>
#include <zypp/base/IOStream.h>
#include <zypp/base/Functional.h>
#include <zypp/base/Backtrace.h>
#include <zypp/base/LogControl.h>
#include <zypp/PathInfo.h>
#include <zypp/ZConfig.h>

#include <zypp/ZYppFactory.h>
#include <zypp/zypp_detail/ZYppImpl.h>

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <utility>

#include <iostream>

using boost::interprocess::file_lock;
using boost::interprocess::scoped_lock;
using boost::interprocess::sharable_lock;

using std::endl;

namespace zyppintern { void repoVariablesReset(); }	// upon re-acquiring the lock...

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  namespace sighandler
  {
    /// Signal handler logging a stack trace
    template <int SIG>
    class SigBacktraceHandler
    {
      static void backtraceHandler( int sig ) {
        INT << "Error: signal " << SIG << endl << dumpBacktrace << endl;
        base::LogControl::instance().emergencyShutdown();
        ::signal( SIG, lastSigHandler );
      }
      static ::sighandler_t lastSigHandler;
    };
    template <int SIG>
    ::sighandler_t SigBacktraceHandler<SIG>::lastSigHandler { ::signal( SIG, SigBacktraceHandler::backtraceHandler ) };

    // Explicit instantiation installs the handler:
    template class SigBacktraceHandler<SIGSEGV>;
    template class SigBacktraceHandler<SIGABRT>;
  }

  namespace env
  {
    /** Hack to circumvent the currently poor --root support. */
    inline Pathname ZYPP_LOCKFILE_ROOT()
    { return getenv("ZYPP_LOCKFILE_ROOT") ? getenv("ZYPP_LOCKFILE_ROOT") : "/"; }
  }

  ///////////////////////////////////////////////////////////////////
  namespace zypp_readonly_hack
  { /////////////////////////////////////////////////////////////////

    static bool active = getenv("ZYPP_READONLY_HACK");

    ZYPP_API void IWantIt()	// see zypp/zypp_detail/ZYppReadOnlyHack.h
    {
      active = true;
      MIL << "ZYPP_READONLY promised." <<  endl;
    }

    bool IGotIt()
    {
      return active;
    }

    /////////////////////////////////////////////////////////////////
  } // namespace zypp_readonly_hack
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  /// \class ZYppGlobalLock
  /// \brief Our broken global lock
  ///
  ///////////////////////////////////////////////////////////////////
  class ZYppGlobalLock
  {
  public:
    ZYppGlobalLock(Pathname &&lFilePath)
      : _zyppLockFilePath(std::move(lFilePath)), _zyppLockFile(NULL),
        _lockerPid(0), _cleanLock(false) {
      filesystem::assert_dir(_zyppLockFilePath.dirname() );
    }

    ZYppGlobalLock(const ZYppGlobalLock &) = delete;
    ZYppGlobalLock(ZYppGlobalLock &&) = delete;
    ZYppGlobalLock &operator=(const ZYppGlobalLock &) = delete;
    ZYppGlobalLock &operator=(ZYppGlobalLock &&) = delete;

    ~ZYppGlobalLock()
    {
        if ( _cleanLock )
        try {
          // Exception safe access to the lockfile.
          ScopedGuard closeOnReturn( accessLockFile() );
          {
            scoped_lock<file_lock> flock( _zyppLockFileLock );	// aquire write lock
            // Truncate the file rather than deleting it. Other processes may
            // still use it to synchronsize.
            ftruncate( fileno(_zyppLockFile), 0 );
          }
          MIL << "Cleaned lock file. (" << getpid() << ")" << std::endl;
        }
        catch(...) {} // let no exception escape.
    }

    pid_t lockerPid() const
    { return _lockerPid; }

    const std::string & lockerName() const
    { return _lockerName; }

    const Pathname & zyppLockFilePath() const
    { return _zyppLockFilePath; }


  private:
    Pathname	_zyppLockFilePath;
    file_lock	_zyppLockFileLock;
    FILE *	_zyppLockFile;

    pid_t	_lockerPid;
    std::string _lockerName;
    bool	_cleanLock;

  private:
    using ScopedGuard = shared_ptr<void>;

    /** Exception safe access to the lockfile.
     * \code
     *   // Exception safe access to the lockfile.
     *   ScopedGuard closeOnReturn( accessLockFile() );
     * \endcode
     */
    ScopedGuard accessLockFile()
    {
      _openLockFile();
      return ScopedGuard( static_cast<void*>(0),
                          std::bind( std::mem_fn( &ZYppGlobalLock::_closeLockFile ), this ) );
    }

    /** Use \ref accessLockFile. */
    void _openLockFile()
    {
      if ( _zyppLockFile != NULL )
        return;	// is open

      // open pid file rw so we are sure it exist when creating the flock
      _zyppLockFile = fopen( _zyppLockFilePath.c_str(), "a+" );
      if ( _zyppLockFile == NULL )
        ZYPP_THROW( Exception( "Cant open " + _zyppLockFilePath.asString() ) );
      _zyppLockFileLock = _zyppLockFilePath.c_str();
      MIL << "Open lockfile " << _zyppLockFilePath << endl;
    }

    /** Use \ref accessLockFile. */
    void _closeLockFile()
    {
      if ( _zyppLockFile == NULL )
        return;	// is closed

      clearerr( _zyppLockFile );
      fflush( _zyppLockFile );
      // http://www.boost.org/doc/libs/1_50_0/doc/html/interprocess/synchronization_mechanisms.html
      // If you are using a std::fstream/native file handle to write to the file
      // while using file locks on that file, don't close the file before releasing
      // all the locks of the file.
      _zyppLockFileLock = file_lock();
      fclose( _zyppLockFile );
      _zyppLockFile = NULL;
      MIL << "Close lockfile " << _zyppLockFilePath << endl;
    }


    bool isProcessRunning( pid_t pid_r )
    {
      // it is another program, not me, see if it is still running
      Pathname procdir( Pathname("/proc")/str::numstring(pid_r) );
      PathInfo status( procdir );
      MIL << "Checking " <<  status << endl;

      if ( ! status.isDir() )
      {
        DBG << "No such process." << endl;
        return false;
      }

      static char buffer[513];
      buffer[0] = buffer[512] = 0;
      // man proc(5): /proc/[pid]/cmdline is empty if zombie.
      if ( std::ifstream( (procdir/"cmdline").c_str() ).read( buffer, 512 ).gcount() > 0 )
      {
        _lockerName = buffer;
        DBG << "Is running: " <<  _lockerName << endl;
        return true;
      }

      DBG << "In zombie state." << endl;
      return false;
    }

    pid_t readLockFile()
    {
      clearerr( _zyppLockFile );
      fseek( _zyppLockFile, 0, SEEK_SET );
      long readpid = 0;
      fscanf( _zyppLockFile, "%ld", &readpid );
      MIL << "read: Lockfile " << _zyppLockFilePath << " has pid " << readpid << " (our pid: " << getpid() << ") "<< std::endl;
      return (pid_t)readpid;
    }

    void writeLockFile()
    {
      clearerr( _zyppLockFile );
      fseek( _zyppLockFile, 0, SEEK_SET );
      ftruncate( fileno(_zyppLockFile), 0 );
      fprintf(_zyppLockFile, "%ld\n", (long)getpid() );
      fflush( _zyppLockFile );
      _cleanLock = true; // cleanup on exit
      MIL << "write: Lockfile " << _zyppLockFilePath << " got pid " <<  getpid() << std::endl;
    }

    /*!
     * Expects the calling function to lock the access lock
     */
    bool safeCheckIsLocked()
    {
      _lockerPid = readLockFile();
      if ( _lockerPid == 0 ) {
        // no or empty lock file
        return false;
      } else if ( _lockerPid == getpid() ) {
        // keep my own lock
        return false;
      } else {
        // a foreign pid in lock
        if ( isProcessRunning( _lockerPid ) ) {
          WAR << _lockerPid << " is running and has a ZYpp lock. Sorry." << std::endl;
          return true;
        } else {
          MIL << _lockerPid << " is dead. Ignoring the existing lock file." << std::endl;
          return false;
        }
      }
    }

  public:

    bool isZyppLocked()
    {
      if ( geteuid() != 0 )
        return false;	// no lock as non-root

      // Exception safe access to the lockfile.
      ScopedGuard closeOnReturn( accessLockFile() );
      scoped_lock<file_lock> flock( _zyppLockFileLock );	// aquire write lock
      return safeCheckIsLocked ();
    }

    /** Try to aquire a lock.
     * \return \c true if zypp is already locked by another process.
     */
    bool zyppLocked()
    {
      if ( geteuid() != 0 )
        return false;	// no lock as non-root

      // Exception safe access to the lockfile.
      ScopedGuard closeOnReturn( accessLockFile() );
      scoped_lock<file_lock> flock( _zyppLockFileLock );	// aquire write lock
      if ( !safeCheckIsLocked() ) {
        writeLockFile();
        return false;
      }
      return true;
    }

  };

  ///////////////////////////////////////////////////////////////////
  namespace
  {
    static weak_ptr<ZYpp>		_theZYppInstance;
    static scoped_ptr<ZYppGlobalLock>	_theGlobalLock;		// on/off in sync with _theZYppInstance

    ZYppGlobalLock & globalLock()
    {
      if ( !_theGlobalLock )
        _theGlobalLock.reset( new ZYppGlobalLock( ZYppFactory::lockfileDir() / "zypp.pid" ) );
      return *_theGlobalLock;
    }
  } //namespace
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : ZYpp
  //
  ///////////////////////////////////////////////////////////////////

  ZYpp::ZYpp( const Impl_Ptr & impl_r )
  : _pimpl( impl_r )
  {
    ::zyppintern::repoVariablesReset();	// upon re-acquiring the lock...
    MIL << "ZYpp is on..." << endl;
  }

  ZYpp::~ZYpp()
  {
    _theGlobalLock.reset();
    MIL << "ZYpp is off..." << endl;
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : ZYppFactoryException
  //
  ///////////////////////////////////////////////////////////////////

  ZYppFactoryException::ZYppFactoryException( std::string msg_r, pid_t lockerPid_r, std::string lockerName_r )
    : Exception( std::move(msg_r) )
    , _lockerPid( lockerPid_r )
    , _lockerName(std::move( lockerName_r ))
  {}

  ZYppFactoryException::~ZYppFactoryException() throw ()
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : ZYppFactory
  //
  ///////////////////////////////////////////////////////////////////

  ZYppFactory ZYppFactory::instance()
  { return ZYppFactory(); }

  ZYppFactory::ZYppFactory()
  {}

  ZYppFactory::~ZYppFactory()
  {}

  ///////////////////////////////////////////////////////////////////
  //
  ZYpp::Ptr ZYppFactory::getZYpp() const
  {

    const auto &makeLockedError = []( pid_t pid, const std::string &lockerName ){
      const std::string &t = str::form(_("System management is locked by the application with pid %d (%s).\n"
                                           "Close this application before trying again."), pid, lockerName.c_str() );
      return ZYppFactoryException(t, pid, lockerName );
    };

    ZYpp::Ptr _instance = _theZYppInstance.lock();
    if ( ! _instance )
    {
      if ( geteuid() != 0 )
      {
        MIL << "Running as user. Skip creating " << globalLock().zyppLockFilePath() << std::endl;
      }
      else if ( zypp_readonly_hack::active )
      {
        MIL << "ZYPP_READONLY active." << endl;
      }
      else if ( globalLock().zyppLocked() )
      {
        bool failed = true;
        // bsc#1184399,1213231: A negative ZYPP_LOCK_TIMEOUT will wait forever.
        const long LOCK_TIMEOUT = ZConfig::instance().lockTimeout();
        if ( LOCK_TIMEOUT != 0 )
        {
          Date logwait = Date::now();
          Date giveup; /* 0 = forever */
          if ( LOCK_TIMEOUT > 0 ) {
            giveup = logwait+LOCK_TIMEOUT;
            MIL << "$ZYPP_LOCK_TIMEOUT=" << LOCK_TIMEOUT << " sec. Waiting for the zypp lock until " << giveup << endl;
          }
          else
            MIL << "$ZYPP_LOCK_TIMEOUT=" << LOCK_TIMEOUT << " sec. Waiting for the zypp lock..." << endl;

          unsigned delay = 0;
          do {
            if ( delay < 60 )
              delay += 1;
            else {
              Date now { Date::now() };
              if ( now - logwait > Date::day ) {
                WAR << "$ZYPP_LOCK_TIMEOUT=" << LOCK_TIMEOUT << " sec. Another day has passed waiting for the zypp lock..." << endl;
                logwait = now;
              }
            }
            sleep( delay );
            {
              zypp::base::LogControl::TmpLineWriter shutUp;     // be quiet
              failed = globalLock().zyppLocked();
            }
          } while ( failed && ( not giveup || Date::now() <= giveup ) );

          if ( failed ) {
            MIL << "$ZYPP_LOCK_TIMEOUT=" << LOCK_TIMEOUT << " sec. Gave up waiting for the zypp lock." << endl;
          }
          else {
            MIL << "$ZYPP_LOCK_TIMEOUT=" << LOCK_TIMEOUT << " sec. Finally got the zypp lock." << endl;
          }
        }
        if ( failed )
          ZYPP_THROW( makeLockedError( globalLock().lockerPid(), globalLock().lockerName() ));

        // we got the global lock, now make sure zypp-rpm is not still running
        {
          ZYppGlobalLock zyppRpmLock( ZYppFactory::lockfileDir() / "zypp-rpm.pid" );
          if ( zyppRpmLock.isZyppLocked () ) {
            // release global lock, we will exit now
            _theGlobalLock.reset();
            ZYPP_THROW( makeLockedError( zyppRpmLock.lockerPid(), zyppRpmLock.lockerName() ));
          }
        }
      }

      // Here we go...
      static ZYpp::Impl_Ptr _theImplInstance;	// for now created once
      if ( !_theImplInstance )
        _theImplInstance.reset( new ZYpp::Impl );
      _instance.reset( new ZYpp( _theImplInstance ) );
      _theZYppInstance = _instance;
    }

    return _instance;
  }

  ///////////////////////////////////////////////////////////////////
  //
  bool ZYppFactory::haveZYpp() const
  { return !_theZYppInstance.expired(); }

  zypp::Pathname ZYppFactory::lockfileDir()
  {
    return env::ZYPP_LOCKFILE_ROOT() / "run";
  }

  /******************************************************************
  **
  **	FUNCTION NAME : operator<<
  **	FUNCTION TYPE : std::ostream &
  */
  std::ostream & operator<<( std::ostream & str, const ZYppFactory & obj )
  {
    return str << "ZYppFactory";
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
