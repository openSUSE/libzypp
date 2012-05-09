/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/target/HistoryLog.h
 *
 */
#ifndef ZYPP_TARGET_COMMITLOG_H
#define ZYPP_TARGET_COMMITLOG_H

#include <iosfwd>

#include "zypp/base/Macros.h"
#include "zypp/Pathname.h"

namespace zypp
{
  class PoolItem;
  class RepoInfo;

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : HistoryLog
  /**
   * Simple wrapper for progress log. Refcnt, filename and corresponding
   * ofstream are static members. Logfile constructor raises, destructor
   * lowers refcounter. On refcounter changing from 0->1, file is opened.
   * Changing from 1->0 the file is closed. Thus Logfile objects should be
   * local to those functions, writing the log, and must not be stored
   * permanently.
   *
   * Usage:
   * <code>
   *  some method ()
   *  {
   *    PoolItem pi;
   *    ...
   *    HistoryLog().install(pi);
   *    ...
   *    HistoryLog().comment(someMessage);
   *  }
   * </code>
   *
   * \note Take care to set proper target root dir if needed. Either pass
   *       it via the constructor, or set it via setRoot(Pathname) method.
   *       The default location of the file is determined by
   *       \ref ZConfig::historyLogPath() which defaults to
   *       /var/log/zypp/history.
   *
   * \see http://en.opensuse.org/Libzypp/Package_History
   *
   * \todo The implementation as pseudo signleton is questionable.
   * Use shared_ptr instead of handcrafted ref/unref. Manage multiple
   * logs at different locations.
   */
  class ZYPP_API HistoryLog
  {
    HistoryLog( const HistoryLog & );
    HistoryLog & operator=( const HistoryLog & );
  public:
    /**
     * Constructor with an optional root directory.
     *
     * \param rootdir actual target root directory
     */
    HistoryLog( const Pathname & rootdir = Pathname() );
    ~HistoryLog();

    /**
     * Set new root directory to the default history log file path.
     *
     * This path will be prepended to the default log file path. This should
     * be done where there is a potential that the target root has changed.
     *
     * \param root new root directory.
     */
    static void setRoot( const Pathname & root );

    /**
     * Get the current log file path.
     */
    static const Pathname & fname();

    /**
     * Log a comment (even multiline).
     *
     * \param comment the comment
     * \param timestamp whether to include a timestamp at the start of the comment
     */
    void comment( const std::string & comment, bool timestamp = false );

    /**
     * Log installation (or update) of a package.
     */
    void install( const PoolItem & pi );

    /**
     * Log removal of a package
     */
    void remove( const PoolItem & pi );

    /**
     * Log a newly added repository.
     *
     * \param repo info about the added repository
     */
    void addRepository( const RepoInfo & repo );

    /**
     * Log recently removed repository.
     *
     * \param repo info about the removed repository
     */
    void removeRepository( const RepoInfo & repo );

    /**
     * Log certain modifications to a repository.
     *
     * \param oldrepo info about the old repository
     * \param newrepo info about the new repository
     */
    void modifyRepository( const RepoInfo & oldrepo, const RepoInfo & newrepo );
  };
  ///////////////////////////////////////////////////////////////////

} // namespace zypp

#endif // ZYPP_TARGET_COMMITLOG_H
