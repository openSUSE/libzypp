/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/target/rpm/RpmDb.h
 *
*/

// -*- C++ -*-

#ifndef ZYPP_TARGET_RPM_RPMDB_H
#define ZYPP_TARGET_RPM_RPMDB_H

#include <iosfwd>
#include <list>
#include <vector>
#include <string>

#include <zypp/Pathname.h>
#include <zypp/ExternalProgram.h>

#include <zypp/Package.h>
#include <zypp/KeyRing.h>

#include <zypp/target/rpm/RpmFlags.h>
#include <zypp/target/rpm/RpmHeader.h>
#include <zypp/target/rpm/RpmCallbacks.h>
#include <zypp/ZYppCallbacks.h>

namespace zypp
{
namespace target
{
namespace rpm
{

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : RpmDb
/**
 * @short Interface to the rpm program
 **/
class RpmDb : public base::ReferenceCounted, private base::NonCopyable
{
public:

  /**
   * Default error class
   **/
  typedef class InstTargetError Error;

  ///////////////////////////////////////////////////////////////////
  //
  // INITALISATION
  //
  ///////////////////////////////////////////////////////////////////
private:

  /**
   * Root directory for all operations.
   **/
  Pathname _root;

  /**
   * Directory that contains the rpmdb.
   **/
  Pathname _dbPath;

public:

  /**
   * Constructor. There's no rpmdb access until @ref initDatabase
   * was called.
   **/
  RpmDb();

  /**
   * Destructor.
   **/
  ~RpmDb();

  /**
   * @return Root directory for all operations (empty if not initialized).
   **/
  const Pathname & root() const
  {
    return _root;
  }

  /**
   * @return Directory that contains the rpmdb (empty if not initialized).
   **/
  const Pathname & dbPath() const
  {
    return _dbPath;
  }

  /**
   * @return Whether we are initialized.
   **/
  bool initialized() const
  {
    return( ! _root.empty() );
  }

  /**
   * Prepare access to the rpm database at \c/var/lib/rpm.
   *
   * An optional argument denotes the root directory for all operations. If
   * an empty Pathname is given the default (\c/) is used.
   *
   * Calling initDatabase a second time with different arguments will return
   * an error but leave the database in it's original state.
   *
   * If the  database already exists and \c doRebuild_r is true,
   * \ref rebuildDatabase is called.
   *
   * \throws RpmException
   **/
  void initDatabase( Pathname root_r = Pathname(), bool doRebuild_r = false );

  /**
   * Block further access to the rpm database and go back to uninitialized
   * state. On update: Decides what to do with any converted database
   * (see @ref initDatabase).
   *
   * \throws RpmException
   *
   **/
  void closeDatabase();

  /**
   * Rebuild the rpm database (rpm --rebuilddb).
   *
   * \throws RpmException
   *
   **/
  void rebuildDatabase();

  /**
   * Import ascii armored public key in file pubkey_r.
   *
   * \throws RpmException
   *
   **/
  void importPubkey( const PublicKey & pubkey_r );

  /**
   * Remove a public key from the rpm database
   *
   * \throws RpmException
   *
   **/
  void removePubkey( const PublicKey & pubkey_r );

  /**
   * Return the long ids of all installed public keys.
   **/
  std::list<PublicKey> pubkeys() const;

  /**
   * Return the edition of all installed public keys.
   **/
  std::set<Edition> pubkeyEditions() const;

  ///////////////////////////////////////////////////////////////////
  //
  // Direct RPM database retrieval via librpm.
  //
  ///////////////////////////////////////////////////////////////////
public:

  /**
   * return complete file list for installed package name_r (in FileInfo.filename)
   * if edition_r != Edition::noedition, check for exact edition
   * if full==true, fill all attributes of FileInfo
   **/
  std::list<FileInfo> fileList( const std::string & name_r, const Edition & edition_r ) const;

  /**
   * Return true if at least one package owns a certain file (name_r empty)
   * Return true if package name_r owns file file_r (name_r nonempty).
   **/
  bool hasFile( const std::string & file_r, const std::string & name_r = "" ) const;

  /**
   * Return name of package owning file
   * or empty string if no installed package owns file
   **/
  std::string whoOwnsFile( const std::string & file_r ) const;

  /**
   * Return true if at least one package provides a certain tag.
   **/
  bool hasProvides( const std::string & tag_r ) const;

  /**
   * Return true if at least one package requires a certain tag.
   **/
  bool hasRequiredBy( const std::string & tag_r ) const;

  /**
   * Return true if at least one package conflicts with a certain tag.
   **/
  bool hasConflicts( const std::string & tag_r ) const;

  /**
   * Return true if package is installed.
   **/
  bool hasPackage( const std::string & name_r ) const;

  /**
   * Return true if package is installed in a certain edition.
   **/
  bool hasPackage( const std::string & name_r, const Edition & ed_r ) const;

  /**
   * Get an installed packages data from rpmdb. Package is
   * identified by name. Data returned via result are NULL,
   * if packge is not installed (PMError is not set), or RPM database
   * could not be read (PMError is set).
   *
   * \throws RpmException
   *
   * FIXME this and following comment
   *
   **/
  void getData( const std::string & name_r,
                RpmHeader::constPtr & result_r ) const;

  /**
   * Get an installed packages data from rpmdb. Package is
   * identified by name and edition. Data returned via result are NULL,
   * if packge is not installed (PMError is not set), or RPM database
   * could not be read (PMError is set).
   *
   * \throws RpmException
   *
   **/
  void getData( const std::string & name_r, const Edition & ed_r,
                RpmHeader::constPtr & result_r ) const;

  ///////////////////////////////////////////////////////////////////
  //
  ///////////////////////////////////////////////////////////////////
public:
  /** Sync mode for \ref syncTrustedKeys */
  enum SyncTrustedKeyBits
  {
    SYNC_TO_KEYRING	= 1<<0,	//!< export rpm trusted keys into zypp trusted keyring
    SYNC_FROM_KEYRING	= 1<<1,	//!< import zypp trusted keys into rpm database.
    SYNC_BOTH		= SYNC_TO_KEYRING | SYNC_FROM_KEYRING
  };
  /**
   * Sync trusted keys stored in rpm database and zypp trusted keyring.
   */
  void syncTrustedKeys( SyncTrustedKeyBits mode_r = SYNC_BOTH );
  /**
   * iterates through zypp keyring and import all non existant keys
   * into rpm keyring
   */
  void importZyppKeyRingTrustedKeys();
  /**
   * insert all rpm trusted keys into zypp trusted keyring
   */
  void exportTrustedKeysInZyppKeyRing();

private:
  /**
   * The connection to the rpm process.
  */
  ExternalProgram *process;

  typedef std::vector<const char*> RpmArgVec;

  /**
   * Run rpm with the specified arguments and handle stderr.
   * @param n_opts The number of arguments
   * @param options Array of the arguments, @ref n_opts elements
   * @param stderr_disp How to handle stderr, merged with stdout by default
   *
   * \throws RpmException
   *
   **/
  void run_rpm( const RpmArgVec& options,
                ExternalProgram::Stderr_Disposition stderr_disp =
                  ExternalProgram::Stderr_To_Stdout);


  /**
   * Read a line from the general rpm query
  */
  bool systemReadLine(std::string &line);

  /**
   * Return the exit status of the general rpm process,
   * closing the connection if not already done.
  */
  int systemStatus();

  /**
   * Forcably kill the system process
  */
  void systemKill();

  /**
   * The exit code of the rpm process, or -1 if not yet known.
  */
  int exit_code;

  /**
   * Error message from running rpm as external program.
   * Use only if something fail.
   */
  std::string error_message;

  /** /var/adm/backup */
  Pathname _backuppath;

  /** create package backups? */
  bool _packagebackups;

  /**
   * handle rpm messages like "/etc/testrc saved as /etc/testrc.rpmorig"
   *
   * @param line rpm output starting with warning:
   * @param name name of package, appears in subject line
   * @param typemsg " saved as " or " created as "
   * @param difffailmsg what to put into mail if diff failed, must contain two %s for the two files
   * @param diffgenmsg what to put into mail if diff succeeded, must contain two %s for the two files
   * */
  void processConfigFiles(const std::string& line,
                          const std::string& name,
                          const char* typemsg,
                          const char* difffailmsg,
                          const char* diffgenmsg);


public:

  typedef std::set<std::string> FileList;

  /**
   * checkPackage result
   * @see checkPackage
   * */
  enum CheckPackageResult
  {
    CHK_OK            = 0, /*!< Signature is OK. */
    CHK_NOTFOUND      = 1, /*!< Signature is unknown type. */
    CHK_FAIL          = 2, /*!< Signature does not verify. */
    CHK_NOTTRUSTED    = 3, /*!< Signature is OK, but key is not trusted. */
    CHK_NOKEY         = 4, /*!< Public key is unavailable. */
    CHK_ERROR         = 5, /*!< File does not exist or can't be opened. */
    CHK_NOSIG         = 6, /*!< File has no gpg signature (only digests). */
  };

  /** Detailed rpm signature check log messages
   * A single multiline message if \ref CHK_OK. Otherwise each message line
   * together with it's \ref CheckPackageResult.
   */
  struct CheckPackageDetail : std::vector<std::pair<CheckPackageResult,std::string>>
  {};

  /**
   * Check signature of rpm file on disk (legacy version returning CHK_OK if file is unsigned, like 'rpm -K')
   *
   * @param path_r which file to check
   * @param detail_r Return detailed rpm log messages
   *
   * @return CheckPackageResult (CHK_OK if file is unsigned)
   *
   * \see also \ref checkPackageSignature
   */
  CheckPackageResult checkPackage( const Pathname & path_r, CheckPackageDetail & detail_r );
  /** \overload Ignoring the \a details_r */
  CheckPackageResult checkPackage( const Pathname & path_r );

  /**
   * Check signature of rpm file on disk (strict check returning CHK_NOSIG if file is unsigned).
   *
   * @param path_r which file to check
   * @param detail_r Return detailed rpm log messages
   *
   * @return CheckPackageResult (CHK_NOSIG if file is unsigned)
   *
   * \see also \ref checkPackage
   */
  CheckPackageResult checkPackageSignature( const Pathname & path_r, CheckPackageDetail & detail_r );

  /** install rpm package
   *
   * @param filename file to install
   * @param flags which rpm options to use
   *
   * @return success
   *
   * \throws RpmException
   *
   * */
  void installPackage ( const Pathname & filename, RpmInstFlags flags = RPMINST_NONE );

  /** remove rpm package
   *
   * @param name_r Name of the rpm package to remove.
   * @param iflags which rpm options to use
   *
   * @return success
   *
   * \throws RpmException
   *
   * */
  void removePackage( const std::string & name_r, RpmInstFlags flags = RPMINST_NONE );
  void removePackage( Package::constPtr package, RpmInstFlags flags = RPMINST_NONE );

  /**
   * get backup dir for rpm config files
   *
   * */
  Pathname getBackupPath (void)
  {
    return _backuppath;
  }

  /**
   * create tar.gz of all changed files in a Package
   *
   * @param packageName name of the Package to backup
   *
   * @see setBackupPath
   * */
  bool backupPackage(const std::string& packageName);

  /**
   * queries file for name and then calls above backupPackage
   * function. For convenience.
   *
   * @param filename rpm file that is about to be installed
   * */
  bool backupPackage(const Pathname& filename);

  /**
   * set path where package backups are stored
   *
   * @see backupPackage
   * */
  void setBackupPath(const Pathname& path);

  /**
   * whether to create package backups during install or
   * removal
   *
   * @param yes true or false
   * */
  void createPackageBackups(bool yes)
  {
    _packagebackups = yes;
  }

  /**
   * determine which files of an installed package have been
   * modified.
   *
   * @param fileList (output) where to store modified files
   * @param packageName name of package to query
   *
   * @return false if package couln't be queried for some
   * reason
   * */
  bool queryChangedFiles(FileList & fileList, const std::string& packageName);

public:

  /**
   * Dump debug info.
   **/
  virtual std::ostream & dumpOn( std::ostream & str ) const;

protected:
  void doRemovePackage( const std::string & name_r, RpmInstFlags flags, callback::SendReport<RpmRemoveReport> & report );
  void doInstallPackage( const Pathname & filename, RpmInstFlags flags, callback::SendReport<RpmInstallReport> & report );
  void doRebuildDatabase(callback::SendReport<RebuildDBReport> & report);
};

/** \relates RpmDb::CheckPackageResult Stream output */
std::ostream & operator<<( std::ostream & str, RpmDb::CheckPackageResult obj );

/** \relates RpmDb::checkPackageDetail Stream output */
std::ostream & operator<<( std::ostream & str, const RpmDb::CheckPackageDetail & obj );

} // namespace rpm
} // namespace target
} // namespace zypp

#endif // ZYPP_TARGET_RPM_RPMDB_H
