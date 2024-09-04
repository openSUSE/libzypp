/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/target/rpm/librpmDb.h
 *
*/
#ifndef librpmDb_h
#define librpmDb_h

#include <iosfwd>

#include <zypp/base/ReferenceCounted.h>
#include <zypp/base/NonCopyable.h>
#include <zypp/base/PtrTypes.h>
#include <zypp/PathInfo.h>
#include <zypp/target/rpm/RpmHeader.h>
#include <zypp/target/rpm/RpmException.h>

namespace zypp
{
namespace target
{
namespace rpm
{
  struct _dumpPath {
    _dumpPath( const Pathname & root_r, const Pathname & sub_r )
    : _root {root_r }
    , _sub { sub_r }
    {}
    const Pathname & _root;
    const Pathname & _sub;
  };
  inline std::ostream & operator<<( std::ostream & str, const _dumpPath & obj )
  { return str << "'(" << obj._root << ")" << obj._sub << "'"; }

  /** dumpPath iomaip to dump '(root_r)sub_r' output, */
  inline _dumpPath dumpPath( const Pathname & root_r, const Pathname & sub_r )
  { return _dumpPath( root_r, sub_r ); }

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : librpmDb
/**
 * @short Manage access to librpm database.
 **/
class librpmDb : public base::ReferenceCounted, private base::NonCopyable
{
public:
  using Ptr = intrusive_ptr<librpmDb>;
  using constPtr = intrusive_ptr<const librpmDb>;

private:
  /**
   * <B>INTENTIONALLY UNDEFINED<\B> because of bug in Ptr classes
   * which allows implicit conversion from librpmDb::Ptr to
   * librpmDb::constPtr. Currently we don't want to provide non const
   * handles, as the database is opened READONLY.
   * \throws RpmException
   **/
  static void dbAccess( librpmDb::Ptr & ptr_r );

public:
  ///////////////////////////////////////////////////////////////////
  //
  //	static interface
  //
  ///////////////////////////////////////////////////////////////////

  /**
   * Initialize lib librpm (read configfiles etc.).
   * It's called on demand but you may call it anytime.
   *
   * @return Whether librpm is initialized.
   **/
  static bool globalInit();

  /**
   * @return librpm macro expansion.
   **/
  static std::string expand( const std::string & macro_r );

  /**
   * \return The preferred location of the rpmdb below \a root_r.
   * It's the location of an already existing db, otherwise the
   * default location suggested by rpm's config.
   *
   * \throws RpmException if root is not an absolute path or no directory for the rpmdb can determined.
   **/
  static Pathname suggestedDbPath( const Pathname & root_r );

  /**
   * \return whether the rpmdb below the system at \a root_r exists.
   * Unless a \a dbPath_r is explicitly specified, the systems default
   * rpmdb is used (\see \ref suggestedDbPath).
   *
   * \throws RpmException if no directory for the rpmdb can be determined.
   */
  static bool dbExists( const Pathname & root_r, const Pathname & dbPath_r = Pathname() );

  /** Open the rpmdb below the system at \a root_r (if it exists).
   * If the database does not exist a \c nullptr is returned.
   * Unless a \a dbPath_r is explicitly specified, the systems default
   * rpmdb is used (\see \ref suggestedDbPath).
   *
   * \throws RpmException if an existing database can not be opened.
   */
  static librpmDb::constPtr dbOpenIf( const Pathname & root_r, const Pathname & dbPath_r = Pathname() );

  /** Assert the rpmdb below the system at \a root_r exists.
   * If the database does not exist an empty one is created.
   * Unless a \a dbPath_r is explicitly specified, the systems default
   * rpmdb is used (\see \ref suggestedDbPath).
   *
   * \throws RpmException if the database can not be created or opened.
   */
  static librpmDb::constPtr dbOpenCreate( const Pathname & root_r, const Pathname & dbPath_r = Pathname() );

  /**
   * Subclass to retrieve database content.
   **/
  class db_const_iterator;

private:

  /**
   * Internal workhorse managing database creation and access.
   * \throws RpmException if an existing database can not be opened.
   */
  static librpmDb::constPtr dbAccess( const Pathname & root_r, const Pathname & dbPath_r, bool create_r = false );

private:
  ///////////////////////////////////////////////////////////////////
  //
  //	internal database handle interface (nonstatic)
  //
  ///////////////////////////////////////////////////////////////////

  /**
   * Hides librpm specific data
   **/
  class D;
  D & _d;

  /**
   * Private constructor! librpmDb objects are to be created via
   * static interface only.
   **/
  librpmDb( const Pathname & root_r, const Pathname & dbPath_r, bool readonly_r = true );

  /**
   * Trigger from @ref Rep, after refCount was decreased.
   **/
  void unref_to( unsigned refCount_r ) const override;

public:

  /**
   * Destructor. Closes rpmdb.
   **/
  ~librpmDb() override;

  /**
   * @return This handles root directory for all operations.
   **/
  const Pathname & root() const;

  /**
   * @return This handles directory that contains the rpmdb.
   **/
  const Pathname & dbPath() const;

public:

  /**
   * Dump debug info.
   **/
  std::ostream & dumpOn( std::ostream & str ) const override;
};

///////////////////////////////////////////////////////////////////
/// \class librpmDb::db_const_iterator
/// \brief Subclass to retrieve rpm database content.
///
/// If the specified rpm database was opened successfully, the iterator
/// is initialized to @ref findAll. Otherwise the iterator is an empty
/// null iterator.
///
/// \note The iterator will never create a not existing database.
/// \note The iterator keeps the rpm database open as a reader, so do not
/// store it for longer than necessary as it may prevent write operations.
///////////////////////////////////////////////////////////////////
class ZYPP_API librpmDb::db_const_iterator
{
  db_const_iterator & operator=( const db_const_iterator & ); // NO ASSIGNMENT!
  db_const_iterator ( const db_const_iterator & );            // NO COPY!
  friend std::ostream & operator<<( std::ostream & str, const db_const_iterator & obj );
  friend class librpmDb;

private:

  /**
   * Hides librpm specific data
   **/
  class D;
  D & _d;

#if LEGACY(1735)
public:
  /**
   * \deprecated Legacy code linked this with 'dbptr_r=0' as default argument.
   * The ZYPP_API is not able to acquire a not-null dbptr_r, so we keep
   * this symbol in the ZYPP_API and map it to the default ctor until
   * we need to raise our SOVERSION. Afterwards this ctor is no longer
   * needed.
   */
  db_const_iterator( librpmDb::constPtr dbptr_r ) ZYPP_DEPRECATED;
#endif

public:
  /** Open the default rpmdb below the host system (at /). */
  db_const_iterator();

  /** Open the default rpmdb below the system at \a root_r. */
  explicit db_const_iterator( const Pathname & root_r );

  /** Open a specific rpmdb below the system at \a root_r. */
  db_const_iterator( const Pathname & root_r, const Pathname & dbPath_r );

  /** A null iterator.
   * What you get if the database does not exist.
   */
  db_const_iterator( std::nullptr_t );

  /**
   * Destructor.
   **/
  ~db_const_iterator();

  /** Whether an underlying rpmdb exists. */
  bool hasDB() const;

  /**
   * Advance to next RpmHeader::constPtr.
   **/
  void operator++();

  /**
   * Returns the current headers index in database,
   * 0 if no header.
   **/
  unsigned dbHdrNum() const;

  /**
   * Returns the current RpmHeader::constPtr or
   * NULL, if no more entries available.
   **/
  const RpmHeader::constPtr & operator*() const;

  /**
   * Forwards to the current RpmHeader::constPtr.
   **/
  const RpmHeader::constPtr & operator->() const
  {
    return operator*();
  }

public:

  /**
   * Reset to iterate all packages. Returns true if iterator
   * contains at least one entry.
   **/
  bool findAll();

  /**
   * Reset to iterate all packages that own a certain file.
   **/
  bool findByFile( const std::string & file_r );

  /**
   * Reset to iterate all packages that provide a certain tag.
   **/
  bool findByProvides( const std::string & tag_r );

  /**
   * Reset to iterate all packages that require a certain tag.
   **/
  bool findByRequiredBy( const std::string & tag_r );

  /**
   * Reset to iterate all packages that conflict with a certain tag.
   **/
  bool findByConflicts( const std::string & tag_r );

  /**
   * Reset to iterate all packages with a certain name.
   *
   * <B>NOTE:</B> Multiple entries for one package installed
   * in different versions are possible but not desired. Usually
   * you'll want to use @ref findPackage instead.
   *
   * findByName is needed to retrieve pseudo packages like
   * 'gpg-pubkey', which in fact exist in multiple instances.
   **/
  bool findByName( const std::string & name_r );

public:

  /**
   * Find package by name.
   *
   * Multiple entries for one package installed in different versions
   * are possible but not desired. If so, the last package installed
   * is returned.
   **/
  bool findPackage( const std::string & name_r );

  /**
   * Find package by name and edition.
   * Commonly used by PMRpmPackageDataProvider.
   **/
  bool findPackage( const std::string & name_r, const Edition & ed_r );

  /**
   * Abbr. for <code>findPackage( which_r->name(), which_r->edition() );</code>
   **/
  bool findPackage( const Package::constPtr & which_r );
};

/** \relates librpmDb::db_const_iterator stream output */
std::ostream & operator<<( std::ostream & str, const librpmDb::db_const_iterator & obj ) ZYPP_API;

///////////////////////////////////////////////////////////////////
} //namespace rpm
} //namespace target
} // namespace zypp

#endif // librpmDb_h

