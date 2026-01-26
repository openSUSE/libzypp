/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/target/rpm/librpmDb.cc
 *
*/
#include "librpm.h"

#include <iostream>
#include <utility>

#include <zypp-core/base/Logger.h>
#include <zypp/PathInfo.h>
#include <zypp-core/AutoDispose.h>
#include <zypp/target/rpm/librpmDb.h>
#include <zypp/target/rpm/RpmHeader.h>
#include <zypp/target/rpm/RpmException.h>

#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "librpmDb"

using std::endl;

namespace zypp
{
namespace target
{
namespace rpm
{
  namespace internal
  {
    // helper functions here expect basic checks on arguments have been performed.
    // (globalInit done, root is absolute, dbpath not empty, ...)
    inline const Pathname & rpmDefaultDbPath()
    {
      static const Pathname _val = [](){
        Pathname ret = librpmDb::expand( "%{_dbpath}" );
        if ( ret.empty() ) {
          ret = "/usr/lib/sysimage/rpm";
          WAR << "Looks like rpm has no %{_dbpath} set!?! Assuming " << ret << endl;
        }
        return ret;
      }();
      return _val;
    }

    inline Pathname suggestedDbPath( const Pathname & root_r )
    {
      if ( PathInfo( root_r ).isDir() ) {
        // If a known dbpath exsists, we continue to use it
        for ( auto p : { "/var/lib/rpm", "/usr/lib/sysimage/rpm" } ) {
          if ( PathInfo( root_r/p, PathInfo::LSTAT/*!no symlink*/ ).isDir() ) {
            MIL << "Suggest existing database at " << dumpPath( root_r, p ) << endl;
            return p;
          }
        }
      }
      const Pathname & defaultDbPath { rpmDefaultDbPath() };
      MIL << "Suggest rpm dbpath " << dumpPath( root_r, defaultDbPath ) << endl;
      return defaultDbPath;
    }

    inline Pathname sanitizedDbPath( const Pathname & root_r, const Pathname & dbPath_r )
    { return dbPath_r.empty() ? suggestedDbPath( root_r ) : dbPath_r; }

    inline bool dbExists( const Pathname & root_r, const Pathname & dbPath_r )
    {
      Pathname dbdir { root_r / sanitizedDbPath( root_r, dbPath_r ) };
      return PathInfo(dbdir).isDir() &&
      ( PathInfo(dbdir/"Packages").isFile() || PathInfo(dbdir/"Packages.db").isFile() || PathInfo(dbdir/"rpmdb.sqlite").isFile() );
    }

  } // namespace internal

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : librpmDb (ststic interface)
//
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : librpmDb::D
/**
 * @short librpmDb internal database handle
 **/
class librpmDb::D
{
  D ( const D & ) = delete;            // NO COPY!
  D & operator=( const D & ) = delete; // NO ASSIGNMENT!
  D(D &&) = delete;
  D &operator=(D &&) = delete;
public:

  const Pathname _root;   // root directory for all operations
  const Pathname _dbPath; // directory (below root) that contains the rpmdb
  AutoDispose<rpmts> _ts; // transaction handle, includes database

  friend std::ostream & operator<<( std::ostream & str, const D & obj )
  { return str << "{" << dumpPath( obj._root, obj._dbPath ) << "}"; }

  D( Pathname root_r, Pathname dbPath_r, bool readonly_r )
  : _root   { std::move(root_r) }
  , _dbPath { std::move(dbPath_r) }
  , _ts     { nullptr }
  {
    _ts = AutoDispose<rpmts>( ::rpmtsCreate(), ::rpmtsFree );
    ::rpmtsSetRootDir( _ts, _root.c_str() );

    // open database (creates a missing one on the fly)
    OnScopeExit cleanup;
    if ( _dbPath != internal::rpmDefaultDbPath() ) {
      // temp set %{_dbpath} macro for rpmtsOpenDB
      cleanup.setDispose( &macroResetDbpath );
      macroSetDbpath( _dbPath );
    }
    int res = ::rpmtsOpenDB( _ts, (readonly_r ? O_RDONLY : O_RDWR ) );
    if ( res ) {
      ERR << "rpmdbOpen error(" << res << "): " << *this << endl;
      ZYPP_THROW(RpmDbOpenException(_root, _dbPath));
    }
  }

private:
  static void macroSetDbpath( const Pathname & dppath_r )
  { ::addMacro( NULL, "_dbpath", NULL, dppath_r.asString().c_str(), RMIL_CMDLINE ); }

  static void macroResetDbpath()
  { macroSetDbpath( internal::rpmDefaultDbPath() ); }
};

///////////////////////////////////////////////////////////////////

bool librpmDb::globalInit()
{
  static bool initialized = false;

  if ( initialized )
    return true;

  int rc = ::rpmReadConfigFiles( NULL, NULL );
  if ( rc )
  {
    ERR << "rpmReadConfigFiles returned " << rc << endl;
    return false;
  }

  initialized = true; // Necessary to be able to use exand() in rpmDefaultDbPath().
  const Pathname & rpmDefaultDbPath { internal::rpmDefaultDbPath() };  // init rpmDefaultDbPath()!
  MIL << "librpm init done: (_target:" << expand( "%{_target}" ) << ") (_dbpath:" << rpmDefaultDbPath << ")" << endl;
  return initialized;
}

std::string librpmDb::expand( const std::string & macro_r )
{
  if ( ! globalInit() )
    return macro_r;  // unexpanded

  AutoFREE<char> val = ::rpmExpand( macro_r.c_str(), NULL );
  if ( val )
    return std::string( val );

  return std::string();
}

Pathname librpmDb::suggestedDbPath( const Pathname & root_r )
{
  if ( ! root_r.absolute() )
    ZYPP_THROW(RpmInvalidRootException( root_r, "" ));

  // initialize librpm (for rpmDefaultDbPath)
  if ( ! globalInit() )
    ZYPP_THROW(GlobalRpmInitException());

  return internal::suggestedDbPath( root_r );
}

bool librpmDb::dbExists( const Pathname & root_r, const Pathname & dbPath_r )
{
  if ( ! root_r.absolute() )
    ZYPP_THROW(RpmInvalidRootException( root_r, "" ));

  // initialize librpm (for rpmDefaultDbPath)
  if ( ! globalInit() )
    ZYPP_THROW(GlobalRpmInitException());

  return internal::dbExists( root_r, dbPath_r );
}

librpmDb::constPtr librpmDb::dbOpenIf( const Pathname & root_r, const Pathname & dbPath_r )
{ return dbAccess( root_r, dbPath_r, /*create_r*/false ); }

librpmDb::constPtr librpmDb::dbOpenCreate( const Pathname & root_r, const Pathname & dbPath_r )
{ return dbAccess( root_r, dbPath_r, /*create_r*/true ); }

librpmDb::constPtr librpmDb::dbAccess( const Pathname & root_r, const Pathname & dbPath_rx, bool create_r )
{
  if ( ! root_r.absolute() )
    ZYPP_THROW(RpmInvalidRootException( root_r, "" ));

  // initialize librpm (for rpmDefaultDbPath)
  if ( ! globalInit() )
    ZYPP_THROW(GlobalRpmInitException());

  Pathname dbPath { internal::sanitizedDbPath( root_r, dbPath_rx ) };
  bool dbExists = internal::dbExists( root_r, dbPath );

  if ( not create_r && not dbExists ) {
    WAR << "NoOpen not existing database " << dumpPath( root_r, dbPath ) << endl;
    return nullptr;
  }
  MIL << (dbExists?"Open":"Create") << " database " << dumpPath( root_r, dbPath ) << endl;
  return new librpmDb( root_r, dbPath, /*readonly*/true );
}

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : librpmDb (internal database handle interface (nonstatic))
//
///////////////////////////////////////////////////////////////////

librpmDb::librpmDb( const Pathname & root_r, const Pathname & dbPath_r, bool readonly_r )
: _d( * new D( root_r, dbPath_r, readonly_r ) )
{}

librpmDb::~librpmDb()
{
  MIL << "Close database " << *this << endl;
  delete &_d;
}

void librpmDb::unref_to( unsigned refCount_r ) const
{ return; } // if ( refCount_r == 1 ) { tbd if we hold a static reference as weak ptr. }

const Pathname & librpmDb::root() const
{ return _d._root; }

const Pathname & librpmDb::dbPath() const
{ return _d._dbPath; }

std::ostream & librpmDb::dumpOn( std::ostream & str ) const
{ return ReferenceCounted::dumpOn( str ) << _d; }

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : librpmDb::db_const_iterator::D
//
class librpmDb::db_const_iterator::D
{
  D & operator=( const D & ) = delete; // NO ASSIGNMENT!
  D ( const D & ) = delete;            // NO COPY!
  D(D &&);
  D &operator=(D &&) = delete;

public:

  librpmDb::constPtr              _dbptr;
  AutoDispose<rpmdbMatchIterator> _mi;
  RpmHeader::constPtr             _hptr;

  /** A null iterator */
  D()
  {}

  /** Open a specific rpmdb if it exists. */
  D( const Pathname & root_r, const Pathname & dbPath_r = Pathname() )
  {
    try {
      _dbptr = librpmDb::dbOpenIf( root_r, dbPath_r );
    }
    catch ( const RpmException & excpt_r ) {
      ZYPP_CAUGHT(excpt_r);
    }
    if ( not _dbptr ) {
      WAR << "No database access: " << dumpPath( root_r, dbPath_r ) << endl;
    }
  }

  /**
   * Let iterator access a dbindex file. Call @ref advance to access the
   * 1st element (if present).
   **/
  bool create( int rpmtag, const void * keyp = NULL, size_t keylen = 0 )
  {
    destroy();
    if ( ! _dbptr )
      return false;
    _mi = AutoDispose<rpmdbMatchIterator>( ::rpmtsInitIterator( _dbptr->_d._ts, rpmTag(rpmtag), keyp, keylen ), ::rpmdbFreeIterator );
    return _mi;
  }

  /**
   * Reset iterator. Always returns false.
   **/
  bool destroy()
  {
    _mi.reset();
    _hptr.reset();
    return false;
  }

  /**
   * Advance to the first/next header in iterator. Destroys iterator if
   * no more headers available.
   **/
  bool advance()
  {
    if ( !_mi )
      return false;
    Header h = ::rpmdbNextIterator( _mi );
    if ( ! h )
    {
      destroy();
      return false;
    }
    _hptr = new RpmHeader( h );
    return true;
  }

  /**
   * Access a dbindex file and advance to the 1st header.
   **/
  bool init( int rpmtag, const void * keyp = NULL, size_t keylen = 0 )
  {
    if ( ! create( rpmtag, keyp, keylen ) )
      return false;
    return advance();
  }

  /**
   * Create an itertator that contains the database entry located at
   * off_r, and advance to the 1st header.
   **/
  bool set( int off_r )
  {
    if ( ! create( RPMDBI_PACKAGES ) )
      return false;
#ifdef RPMFILEITERMAX	// since rpm.4.12
    ::rpmdbAppendIterator( _mi, (const unsigned *)&off_r, 1 );
#else
    ::rpmdbAppendIterator( _mi, &off_r, 1 );
#endif
    return advance();
  }

  unsigned offset()
  {
    return( _mi ? ::rpmdbGetIteratorOffset( _mi ) : 0 );
  }

  int size()
  {
    if ( !_mi )
      return 0;
    int ret = ::rpmdbGetIteratorCount( _mi );
    return( ret ? ret : -1 ); // -1: sequential access
  }
};

///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : librpmDb::Ptr::db_const_iterator
//
///////////////////////////////////////////////////////////////////

#if LEGACY(1735)
// Former ZYPP_API used this as default ctor (dbptr_r == nullptr).
// (dbptr_r!=nullptr) is not possible because librpmDb is not in ZYPP_API.
librpmDb::db_const_iterator::db_const_iterator( librpmDb::constPtr dbptr_r )
: db_const_iterator( "/" )
{}
#endif

librpmDb::db_const_iterator::db_const_iterator()
: _d( * new D( "/" ) )
{ findAll(); }

librpmDb::db_const_iterator::db_const_iterator( const Pathname & root_r )
: _d( * new D( root_r ) )
{ findAll(); }

librpmDb::db_const_iterator::db_const_iterator( const Pathname & root_r, const Pathname & dbPath_r )
: _d( * new D( root_r, dbPath_r ) )
{ findAll(); }

librpmDb::db_const_iterator::db_const_iterator( std::nullptr_t )
: _d( * new D() )
{}

librpmDb::db_const_iterator::~db_const_iterator()
{ delete &_d; }

bool librpmDb::db_const_iterator::hasDB() const
{ return bool(_d._dbptr); }

void librpmDb::db_const_iterator::operator++()
{ _d.advance(); }

unsigned librpmDb::db_const_iterator::dbHdrNum() const
{ return _d.offset(); }

const RpmHeader::constPtr & librpmDb::db_const_iterator::operator*() const
{ return _d._hptr; }

std::ostream & operator<<( std::ostream & str, const librpmDb::db_const_iterator & obj )
{ return str << "db_const_iterator(" << obj._d._dbptr << ")"; }

bool librpmDb::db_const_iterator::findAll()
{ return _d.init( RPMDBI_PACKAGES ); }

bool librpmDb::db_const_iterator::findByFile( const std::string & file_r )
{ return _d.init( RPMTAG_BASENAMES, file_r.c_str() ); }

bool librpmDb::db_const_iterator::findByProvides( const std::string & tag_r )
{ return _d.init( RPMTAG_PROVIDENAME, tag_r.c_str() ); }

bool librpmDb::db_const_iterator::findByRequiredBy( const std::string & tag_r )
{ return _d.init( RPMTAG_REQUIRENAME, tag_r.c_str() ); }

bool librpmDb::db_const_iterator::findByConflicts( const std::string & tag_r )
{ return _d.init( RPMTAG_CONFLICTNAME, tag_r.c_str() ); }

bool librpmDb::db_const_iterator::findByName( const std::string & name_r )
{ return _d.init( RPMTAG_NAME, name_r.c_str() ); }

bool librpmDb::db_const_iterator::findPackage( const std::string & name_r )
{
  if ( ! _d.init( RPMTAG_NAME, name_r.c_str() ) )
    return false;

  if ( _d.size() == 1 )
    return true;

  // check installtime on multiple entries
  int match    = 0;
  time_t itime = 0;
  for ( ; operator*(); operator++() )
  {
    if ( operator*()->tag_installtime() > itime )
    {
      match = _d.offset();
      itime = operator*()->tag_installtime();
    }
  }

  return _d.set( match );
}

bool librpmDb::db_const_iterator::findPackage( const std::string & name_r, const Edition & ed_r )
{
  if ( ! _d.init( RPMTAG_NAME, name_r.c_str() ) )
    return false;

  for ( ; operator*(); operator++() )
  {
    if ( ed_r == operator*()->tag_edition() )
    {
      int match = _d.offset();
      return _d.set( match );
    }
  }

  return _d.destroy();
}

bool librpmDb::db_const_iterator::findPackage( const Package::constPtr & which_r )
{
  if ( ! which_r )
    return _d.destroy();

  return findPackage( which_r->name(), which_r->edition() );
}

} // namespace rpm
} // namespace target
} // namespace zypp
