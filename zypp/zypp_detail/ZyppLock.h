/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/zypp_detail/ZYppLock.h
 *
*/
#ifndef ZYPP_ZYPP_DETAIL_ZYPPLOCK_H
#define ZYPP_ZYPP_DETAIL_ZYPPLOCK_H

#include <zypp-core/zyppng/base/zyppglobal.h>
#include <zypp-core/base/PtrTypes.h>
#include <zypp-core/Pathname.h>

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>


namespace zypp {

using boost::interprocess::file_lock;
using boost::interprocess::scoped_lock;
using boost::interprocess::sharable_lock;

ZYPP_FWD_DECL_TYPE_WITH_REFS (ZyppContextLock);

///////////////////////////////////////////////////////////////////
/// \class ZyppContextLock
/// \brief This lock is aquired by a context, currently its a shared "lock everything" type of lock
///        but it will be replaced by a read/write lock, then maybe made even more granular.
///
///////////////////////////////////////////////////////////////////
class ZyppContextLock
{
public:
  ZyppContextLock(Pathname root, const Pathname &fFileName );
  ZyppContextLock( const ZyppContextLock & ) = delete;
  ZyppContextLock(ZyppContextLock &&) = delete;
  ZyppContextLock &operator=(const ZyppContextLock &) = delete;
  ZyppContextLock &operator=(ZyppContextLock &&) = delete;

  ~ZyppContextLock();

  pid_t lockerPid() const;
  const std::string &lockerName() const;
  const Pathname &zyppLockFilePath() const;

  static std::shared_ptr<ZyppContextLock> globalLock( const zypp::Pathname &root  );
  static zypp::Pathname globalLockfileDir( const zypp::Pathname &root );
private:

  Pathname      _zyppLockRoot;      //< root of the context aquiring the lock
  Pathname	_zyppLockFilePath;  //< absolute lockfile path
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
  ScopedGuard accessLockFile();

  /** Use \ref accessLockFile. */
  void _openLockFile();

  /** Use \ref accessLockFile. */
  void _closeLockFile();

  bool isProcessRunning(pid_t pid_r);

  pid_t readLockFile();

  void writeLockFile();

  /*!
   * Expects the calling function to lock the access lock
   */
  bool safeCheckIsLocked();

public:
  bool isZyppLocked();

  /** Try to aquire a lock.
   * \return \c true if zypp is already locked by another process.
   */
  bool zyppLocked();


  /**
   * Aquires the lock, throws if the log could not be acquired.
   * If a lockTimeout is given, zypp will try to keep acquiring the lock
   * untit it either succeeds or that timeout has been reached
   */
  void acquireLock( const long lockTimeout = 0 );
};

}
#endif
