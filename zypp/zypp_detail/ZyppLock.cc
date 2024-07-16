/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "ZyppLock.h"
#include <zypp/base/Gettext.h>
#include <zypp/base/Logger.h>
#include <zypp/base/LogControl.h>
#include <zypp-core/fs/PathInfo.h>

#include <zypp/ZYppFactory.h>

#include <fstream>

namespace zypp {

  namespace zypp_readonly_hack {

    static bool active = getenv("ZYPP_READONLY_HACK");

    ZYPP_API void IWantIt()	// see zypp/zypp_detail/ZYppReadOnlyHack.h
    {
      active = true;
      MIL << "ZYPP_READONLY promised." <<  std::endl;
    }

    bool IGotIt()
    {
      return active;
    }
  } // namespace zypp_readonly_hack

  ZyppContextLock::ZyppContextLock( Pathname root, const Pathname &fFileName )
    : _zyppLockRoot( std::move(root) )
    , _zyppLockFilePath( ZyppContextLock::globalLockfileDir( root ) / fFileName  )
    , _zyppLockFile(NULL)
    , _lockerPid(0)
    , _cleanLock(false) {
    filesystem::assert_dir(_zyppLockFilePath.dirname());
  }

  ZyppContextLock::~ZyppContextLock() {
    if (_cleanLock)
      try {
      // Exception safe access to the lockfile.
      ScopedGuard closeOnReturn(accessLockFile());
      {
        scoped_lock<file_lock> flock(_zyppLockFileLock); // aquire write lock
        // Truncate the file rather than deleting it. Other processes may
        // still use it to synchronize.
        ftruncate(fileno(_zyppLockFile), 0);
      }
      MIL << "Cleaned lock file. (" << getpid() << ")" << std::endl;
    } catch (...) {
    } // let no exception escape.
  }

  pid_t ZyppContextLock::lockerPid() const { return _lockerPid; }
  const std::string &ZyppContextLock::lockerName() const { return _lockerName; }
  const Pathname &ZyppContextLock::zyppLockFilePath() const {
    return _zyppLockFilePath;
  }

  zypp::Pathname ZyppContextLock::globalLockfileDir( const zypp::Pathname &root )
  {
    return root / "run";
  }

  std::shared_ptr<ZyppContextLock> ZyppContextLock::globalLock( const Pathname &root )
  {
    static std::unordered_map<std::string, std::weak_ptr<ZyppContextLock>> globalLocks;

    std::string key;
    if ( root.empty()) {
      key = "/";
    } else {
      key = root.asString();
    }

    auto &rootLock = globalLocks[key];

    auto ptr = rootLock.lock();
    if ( ptr ) {
      return ptr;
    }

    rootLock = ptr = std::make_shared<ZyppContextLock>( root,  "zypp.pid" );
    return ptr;
  }

  ZyppContextLock::ScopedGuard ZyppContextLock::accessLockFile() {
    _openLockFile();
    return ScopedGuard(
          static_cast<void *>(0),
          std::bind(std::mem_fn(&ZyppContextLock::_closeLockFile), this));
  }
  void ZyppContextLock::_openLockFile() {
    if (_zyppLockFile != NULL)
      return; // is open

    // open pid file rw so we are sure it exist when creating the flock
    _zyppLockFile = fopen(_zyppLockFilePath.c_str(), "a+");
    if (_zyppLockFile == NULL)
      ZYPP_THROW(Exception("Cant open " + _zyppLockFilePath.asString()));
    _zyppLockFileLock = _zyppLockFilePath.c_str();
    MIL << "Open lockfile " << _zyppLockFilePath << std::endl;
  }

  void ZyppContextLock::_closeLockFile() {
    if (_zyppLockFile == NULL)
      return; // is closed

    clearerr(_zyppLockFile);
    fflush(_zyppLockFile);
    // http://www.boost.org/doc/libs/1_50_0/doc/html/interprocess/synchronization_mechanisms.html
    // If you are using a std::fstream/native file handle to write to the file
    // while using file locks on that file, don't close the file before
    // releasing all the locks of the file.
    _zyppLockFileLock = file_lock();
    fclose(_zyppLockFile);
    _zyppLockFile = NULL;
    MIL << "Close lockfile " << _zyppLockFilePath << std::endl;
  }

  bool ZyppContextLock::isProcessRunning(pid_t pid_r) {
    // it is another program, not me, see if it is still running
    Pathname procdir(Pathname("/proc") / str::numstring(pid_r));
    PathInfo status(procdir);
    MIL << "Checking " << status << std::endl;

    if (!status.isDir()) {
      DBG << "No such process." << std::endl;
      return false;
    }

    static char buffer[513];
    buffer[0] = buffer[512] = 0;
    // man proc(5): /proc/[pid]/cmdline is empty if zombie.
    if (std::ifstream((procdir / "cmdline").c_str())
        .read(buffer, 512)
        .gcount() > 0) {
      _lockerName = buffer;
      DBG << "Is running: " << _lockerName << std::endl;
      return true;
    }

    DBG << "In zombie state." << std::endl;
    return false;
  }

  pid_t ZyppContextLock::readLockFile() {
    clearerr(_zyppLockFile);
    fseek(_zyppLockFile, 0, SEEK_SET);
    long readpid = 0;
    fscanf(_zyppLockFile, "%ld", &readpid);
    MIL << "read: Lockfile " << _zyppLockFilePath << " has pid " << readpid
        << " (our pid: " << getpid() << ") " << std::endl;
    return (pid_t)readpid;
  }

  void ZyppContextLock::writeLockFile() {
    clearerr(_zyppLockFile);
    fseek(_zyppLockFile, 0, SEEK_SET);
    ftruncate(fileno(_zyppLockFile), 0);
    fprintf(_zyppLockFile, "%ld\n", (long)getpid());
    fflush(_zyppLockFile);
    _cleanLock = true; // cleanup on exit
    MIL << "write: Lockfile " << _zyppLockFilePath << " got pid " << getpid()
        << std::endl;
  }

  bool ZyppContextLock::safeCheckIsLocked() {
    _lockerPid = readLockFile();
    if (_lockerPid == 0) {
      // no or empty lock file
      return false;
    } else if (_lockerPid == getpid()) {
      // keep my own lock
      return false;
    } else {
      // a foreign pid in lock
      if (isProcessRunning(_lockerPid)) {
        WAR << _lockerPid << " is running and has a ZYpp lock. Sorry."
            << std::endl;
        return true;
      } else {
        MIL << _lockerPid << " is dead. Ignoring the existing lock file."
            << std::endl;
        return false;
      }
    }
  }

  bool ZyppContextLock::isZyppLocked() {
    if (geteuid() != 0)
      return false; // no lock as non-root

    // Exception safe access to the lockfile.
    ScopedGuard closeOnReturn(accessLockFile());
    scoped_lock<file_lock> flock(_zyppLockFileLock); // aquire write lock
    return safeCheckIsLocked();
  }

  bool ZyppContextLock::zyppLocked() {
    if (geteuid() != 0)
      return false; // no lock as non-root

    // Exception safe access to the lockfile.
    ScopedGuard closeOnReturn(accessLockFile());
    scoped_lock<file_lock> flock(_zyppLockFileLock); // aquire write lock
    if (!safeCheckIsLocked()) {
      writeLockFile();
      return false;
    }
    return true;
  }

  void ZyppContextLock::acquireLock( const long lockTimeout )
  {

    const auto &makeLockedError = [](pid_t pid, const std::string &lockerName) {
      const std::string &t = str::form(
            _("System management is locked by the application with pid %d (%s).\n"
              "Close this application before trying again."),
            pid, lockerName.c_str());

      return ZYppFactoryException(t, pid, lockerName);
    };

    if ( geteuid() != 0 )
    {
      MIL << "Running as user. Skip creating " << zyppLockFilePath() << std::endl;
    }
    else if ( zypp_readonly_hack::active )
    {
      MIL << "ZYPP_READONLY active." << std::endl;
    }
    else if ( zyppLocked() )
    {
      bool failed = true;
      // bsc#1184399,1213231: A negative ZYPP_LOCK_TIMEOUT will wait forever.
      if ( lockTimeout != 0 )
      {
        Date logwait = Date::now();
        Date giveup; /* 0 = forever */
        if ( lockTimeout > 0 ) {
          giveup = logwait+lockTimeout;
          MIL << "$ZYPP_LOCK_TIMEOUT=" << lockTimeout << " sec. Waiting for the zypp lock until " << giveup << std::endl;
        }
        else
          MIL << "$ZYPP_LOCK_TIMEOUT=" << lockTimeout << " sec. Waiting for the zypp lock..." << std::endl;

        unsigned delay = 0;
        do {
          if ( delay < 60 )
            delay += 1;
          else {
            Date now { Date::now() };
            if ( now - logwait > Date::day ) {
              WAR << "$ZYPP_LOCK_TIMEOUT=" << lockTimeout << " sec. Another day has passed waiting for the zypp lock..." << std::endl;
              logwait = now;
            }
          }
          sleep( delay );
          {
            zypp::base::LogControl::TmpLineWriter shutUp;     // be quiet
            failed = zyppLocked();
          }
        } while ( failed && ( not giveup || Date::now() <= giveup ) );

        if ( failed ) {
          MIL << "$ZYPP_LOCK_TIMEOUT=" << lockTimeout << " sec. Gave up waiting for the zypp lock." << std::endl;
        }
        else {
          MIL << "$ZYPP_LOCK_TIMEOUT=" << lockTimeout << " sec. Finally got the zypp lock." << std::endl;
        }
      }
      if ( failed )
        ZYPP_THROW( makeLockedError( lockerPid(), lockerName() ));

      // we got the base lock, now make sure zypp-rpm is not still running
      {
        ZyppContextLock zyppRpmLock( _zyppLockRoot, "zypp-rpm.pid" );
        if ( zyppRpmLock.isZyppLocked () ) {
          ZYPP_THROW( makeLockedError( zyppRpmLock.lockerPid(), zyppRpmLock.lockerName() ));
        }
      }
    }
  }
} // namespace zypp
