/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/misc/CheckAccessDeleted.cc
 *
*/
#include <iostream>
#include <fstream>
#include <unordered_set>
#include <iterator>
#include <stdio.h>
#include "zypp/base/LogTools.h"
#include "zypp/base/String.h"
#include "zypp/base/Gettext.h"
#include "zypp/base/Exception.h"

#include "zypp/PathInfo.h"
#include "zypp/ExternalProgram.h"
#include "zypp/base/Regex.h"
#include "zypp/base/IOStream.h"
#include "zypp/base/InputStream.h"
#include "zypp/target/rpm/librpmDb.h"

#include "zypp/misc/CheckAccessDeleted.h"

using std::endl;

#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::misc"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  namespace
  { /////////////////////////////////////////////////////////////////
    //
    // lsof output lines are a sequence of NUL terminated fields,
    // where the 1st char determines the fields type.
    //
    // (pcuL) pid command userid loginname
    // (ftkn).filedescriptor type linkcount filename
    //
    /////////////////////////////////////////////////////////////////

    /** lsof output line + files extracted so far for this PID */
    typedef std::pair<std::string,std::unordered_set<std::string>> CacheEntry;

    /////////////////////////////////////////////////////////////////
    /// \class FilterRunsInContainer
    /// \brief Functor guessing whether \a PID is running in a container.
    ///
    /// Use /proc to guess if a process is running in a container
    /////////////////////////////////////////////////////////////////
    struct FilterRunsInContainer
    {
    private:

      enum Type {
        IGNORE,
        HOST,
        CONTAINER
      };

      /*!
       * Checks if the given file in proc is part of our root
       * or not. If the file was unlinked IGNORE is returned to signal
       * that its better to check the next file.
       */
      Type in_our_root( const Pathname &path ) const {

        const PathInfo procInfoStat( path );

        // if we can not stat the file continue to the next one
        if ( procInfoStat.error() ) return IGNORE;

        // if the file was unlinked ignore it
        if ( procInfoStat.nlink() == 0 )
          return IGNORE;

        // get the file the link points to, if that fails continue to the next
        const Pathname linkTarget = filesystem::readlink( path );
        if ( linkTarget.empty() ) return IGNORE;

        // get stat info for the target file
        const PathInfo linkStat( linkTarget );

        // Non-existent path means it's not reachable by us.
        if ( !linkStat.isExist() )
          return CONTAINER;

        // If the file exists, it could simply mean it exists in and outside a container, check inode to be safe
        if ( linkStat.ino() != procInfoStat.ino())
          return CONTAINER;

        // If the inode is the same, it could simply mean it exists in and outside a container but on different devices, check to be safe
        if ( linkStat.dev() != procInfoStat.dev() )
          return CONTAINER;

        // assume HOST if all tests fail
        return HOST;
      }

    public:

      /*!
       * Iterates over the /proc contents for the given pid
       */
      bool operator()( const pid_t pid ) const {

        // first check the exe file
        const Pathname pidDir  = Pathname("/proc") / asString(pid);
        const Pathname exeFile = pidDir / "exe";

        auto res = in_our_root( exeFile );
        if ( res > IGNORE )
          return res == CONTAINER;

        // if IGNORE was returned we need to continue testing all the files in /proc/<pid>/map_files until we hopefully
        // find a still existing file. If all tests fail we will simply assume this pid is running on the HOST

        // a map of all already tested files, each file can be mapped multiple times and we do not want to check them more than once
        std::unordered_set<std::string> tested;

        // iterate over all the entries in /proc/<pid>/map_files
        filesystem::dirForEach( pidDir / "map_files", [ this, &tested, &res ]( const Pathname & dir_r, const char *const & name_r  ){

          // some helpers to make the code more self explanatory
          constexpr bool contloop = true;
          constexpr bool stoploop = false;

          const Pathname entryName = dir_r / name_r;

          // get the links target file and check if we alreadys know it, also if we can not read link information we skip the file
          const Pathname linkTarget = filesystem::readlink( entryName );
          if ( linkTarget.empty() || !tested.insert( linkTarget.asString() ).second ) return contloop;

          // try to get file type
          const auto mappedFileType = in_our_root( entryName );

          // if we got something, remember the value and stop the loop
          if ( mappedFileType > IGNORE ) {
            res = mappedFileType;
            return stoploop;
          }
          return contloop;
        });

        // if res is still IGNORE we did not find a explicit answer. So to be safe we assume its running on the host
        if ( res == IGNORE )
          return false; // can't tell for sure, lets assume host

        return res == CONTAINER;
      }

      FilterRunsInContainer() {}
    };


    /** bsc#1099847: Check for lsof version < 4.90 which does not support '-K i'
     * Just a quick check to allow code15 libzypp runnig in a code12 environment.
     * bsc#1036304: '-K i' was backported to older lsof versions, indicated by
     * lsof providing 'backported-option-Ki'.
     */
    bool lsofNoOptKi()
    {
      using target::rpm::librpmDb;
      // RpmDb access is blocked while the Target is not initialized.
      // Launching the Target just for this query would be an overkill.
      struct TmpUnblock {
	TmpUnblock()
	: _wasBlocked( librpmDb::isBlocked() )
	{ if ( _wasBlocked ) librpmDb::unblockAccess(); }
	~TmpUnblock()
	{ if ( _wasBlocked ) librpmDb::blockAccess(); }
      private:
	bool _wasBlocked;
      } tmpUnblock;

      librpmDb::db_const_iterator it;
      return( it.findPackage( "lsof" ) && it->tag_edition() < Edition("4.90") && !it->tag_provides().count( Capability("backported-option-Ki") ) );
    }

  } //namespace
  /////////////////////////////////////////////////////////////////

  class CheckAccessDeleted::Impl
  {
  public:
    CheckAccessDeleted::Impl *clone() const;

    bool addDataIf( const CacheEntry & cache_r, std::vector<std::string> *debMap = nullptr );
    void addCacheIf( CacheEntry & cache_r, const std::string & line_r, std::vector<std::string> *debMap = nullptr );

    std::map<pid_t,CacheEntry> filterInput( externalprogram::ExternalDataSource &source );
    CheckAccessDeleted::size_type createProcInfo( const std::map<pid_t,CacheEntry> &in );

    std::vector<CheckAccessDeleted::ProcInfo> _data;
    bool _fromLsofFileMode = false; // Set if we currently process data from a debug file
    bool _verbose = false;

    std::map<pid_t,std::vector<std::string>> debugMap; //will contain all used lsof files after filtering
    Pathname _debugFile;
  };

  CheckAccessDeleted::Impl *CheckAccessDeleted::Impl::clone() const
  {
    Impl *myClone = new Impl( *this );
    return myClone;
  }

  /** Add \c cache to \c data if the process is accessing deleted files.
   * \c pid string in \c cache is the proc line \c (pcuLR), \c files
   * are already in place. Always clear the \c cache.files!
  */
  inline bool CheckAccessDeleted::Impl::addDataIf( const CacheEntry & cache_r, std::vector<std::string> *debMap )
  {
    const auto & filelist( cache_r.second );

    if ( filelist.empty() )
      return false;

    // at least one file access so keep it:
    _data.push_back( CheckAccessDeleted::ProcInfo() );
    CheckAccessDeleted::ProcInfo & pinfo( _data.back() );
    pinfo.files.insert( pinfo.files.begin(), filelist.begin(), filelist.end() );

    const std::string & pline( cache_r.first );
    std::string commandname;	// pinfo.command if still needed...
    std::ostringstream pLineStr; //rewrite the first line in debug cache
    for_( ch, pline.begin(), pline.end() )
    {
      switch ( *ch )
      {
        case 'p':
          pinfo.pid = &*(ch+1);
          if ( debMap )
            pLineStr <<&*(ch)<<'\0';
          break;
        case 'R':
          pinfo.ppid = &*(ch+1);
          if ( debMap )
            pLineStr <<&*(ch)<<'\0';
          break;
        case 'u':
          pinfo.puid = &*(ch+1);
          if ( debMap )
            pLineStr <<&*(ch)<<'\0';
          break;
        case 'L':
          pinfo.login = &*(ch+1);
          if ( debMap )
            pLineStr <<&*(ch)<<'\0';
          break;
        case 'c':
          if ( pinfo.command.empty() ) {
            commandname = &*(ch+1);
            // the lsof command name might be truncated, so we prefer /proc/<pid>/exe
            if (!_fromLsofFileMode)
              pinfo.command = filesystem::readlink( Pathname("/proc")/pinfo.pid/"exe" ).basename();
            if ( pinfo.command.empty() )
              pinfo.command = std::move(commandname);
            if ( debMap )
              pLineStr <<'c'<<pinfo.command<<'\0';
          }
          break;
      }
      if ( *ch == '\n' ) break;		// end of data
      do { ++ch; } while ( *ch != '\0' );	// skip to next field
    }

    //replace the data in the debug cache as well
    if ( debMap ) {
      pLineStr<<endl;
      debMap->front() = pLineStr.str();
    }

    //entry was added
    return true;
  }


  /** Add file to cache if it refers to a deleted executable or library file:
   * - Either the link count \c(k) is \c 0, or no link cout is present.
   * - The type \c (t) is set to \c REG or \c DEL
   * - The filedescriptor \c (f) is set to \c txt, \c mem or \c DEL
  */
  inline void CheckAccessDeleted::Impl::addCacheIf( CacheEntry & cache_r, const std::string & line_r, std::vector<std::string> *debMap )
  {
    const char * f = 0;
    const char * t = 0;
    const char * n = 0;

    for_( ch, line_r.c_str(), ch+line_r.size() )
    {
      switch ( *ch )
      {
        case 'k':
          if ( *(ch+1) != '0' )	// skip non-zero link counts
            return;
          break;
        case 'f':
          f = ch+1;
          break;
        case 't':
          t = ch+1;
          break;
        case 'n':
          n = ch+1;
          break;
      }
      if ( *ch == '\n' ) break;		// end of data
      do { ++ch; } while ( *ch != '\0' );	// skip to next field
    }

    if ( !t || !f || !n )
      return;	// wrong filedescriptor/type/name

    if ( !(    ( *t == 'R' && *(t+1) == 'E' && *(t+2) == 'G' && *(t+3) == '\0' )
            || ( *t == 'D' && *(t+1) == 'E' && *(t+2) == 'L' && *(t+3) == '\0' ) ) )
      return;	// wrong type

    if ( !(    ( *f == 'm' && *(f+1) == 'e' && *(f+2) == 'm' && *(f+3) == '\0' )
            || ( *f == 't' && *(f+1) == 'x' && *(f+2) == 't' && *(f+3) == '\0' )
            || ( *f == 'D' && *(f+1) == 'E' && *(f+2) == 'L' && *(f+3) == '\0' )
            || ( *f == 'l' && *(f+1) == 't' && *(f+2) == 'x' && *(f+3) == '\0' ) ) )
      return;	// wrong filedescriptor type

    if ( str::contains( n, "(stat: Permission denied)" ) )
      return;	// Avoid reporting false positive due to insufficient permission.

    if ( ! _verbose )
    {
      if ( ! ( str::contains( n, "/lib" ) || str::contains( n, "bin/" ) ) )
        return; // Try to avoid reporting false positive unless verbose.
    }

    if ( *f == 'm' || *f == 'D' )	// skip some wellknown nonlibrary memorymapped files
    {
      static const char * black[] = {
          "/SYSV"
        , "/var/"
        , "/dev/"
        , "/tmp/"
        , "/proc/"
	, "/memfd:"
      };
      for_( it, arrayBegin( black ), arrayEnd( black ) )
      {
        if ( str::hasPrefix( n, *it ) )
          return;
      }
    }
    // Add if no duplicate
    if ( debMap && cache_r.second.find(n) == cache_r.second.end() ) {
      debMap->push_back(line_r);
    }
    cache_r.second.insert( n );
  }

  CheckAccessDeleted::CheckAccessDeleted( bool doCheck_r )
    : _pimpl(new Impl)
  {
    if ( doCheck_r ) check();
  }

  CheckAccessDeleted::size_type CheckAccessDeleted::check( const Pathname &lsofOutput_r, bool verbose_r )
  {
    _pimpl->_verbose = verbose_r;
    _pimpl->_fromLsofFileMode = true;

    FILE *inFile = fopen( lsofOutput_r.c_str(), "r" );
    if ( !inFile ) {
      ZYPP_THROW( Exception(  str::Format("Opening input file %1% failed.") % lsofOutput_r.c_str() ) );
    }

    //inFile is closed by ExternalDataSource
    externalprogram::ExternalDataSource inSource( inFile, nullptr );
    auto cache = _pimpl->filterInput( inSource );
    return _pimpl->createProcInfo( cache );
  }

  std::map<pid_t,CacheEntry> CheckAccessDeleted::Impl::filterInput( externalprogram::ExternalDataSource &source )
  {
    // cachemap: PID => (deleted files)
    // NOTE: omit PIDs running in a (lxc/docker) container
    std::map<pid_t,CacheEntry> cachemap;

    bool debugEnabled = !_debugFile.empty();

    pid_t cachepid = 0;
    FilterRunsInContainer runsInLXC;
    for( std::string line = source.receiveLine(); ! line.empty(); line = source.receiveLine() )
    {
      // NOTE: line contains '\0' separeated fields!
      if ( line[0] == 'p' )
      {
        str::strtonum( line.c_str()+1, cachepid );	// line is "p<PID>\0...."
        if ( _fromLsofFileMode || !runsInLXC( cachepid ) ) {
          if ( debugEnabled ) {
            auto &pidMad = debugMap[cachepid];
            if ( pidMad.empty() )
              debugMap[cachepid].push_back( line );
            else
              debugMap[cachepid].front() = line;
          }
          cachemap[cachepid].first.swap( line );
        } else {
          cachepid = 0;	// ignore this pid
        }
      }
      else if ( cachepid )
      {
        auto &dbgMap = debugMap[cachepid];
        addCacheIf( cachemap[cachepid], line, debugEnabled ? &dbgMap : nullptr);
      }
    }
    return cachemap;
  }

  CheckAccessDeleted::size_type CheckAccessDeleted::check( bool verbose_r  )
  {
    static const char* argv[] = { "lsof", "-n", "-FpcuLRftkn0", "-K", "i", NULL };
    if ( lsofNoOptKi() )
      argv[3] = NULL;

    _pimpl->_verbose = verbose_r;
    _pimpl->_fromLsofFileMode = false;

    ExternalProgram prog( argv, ExternalProgram::Discard_Stderr );
    std::map<pid_t,CacheEntry> cachemap = _pimpl->filterInput( prog );

    int ret = prog.close();
    if ( ret != 0 )
    {
      if ( ret == 129 )
      {
        ZYPP_THROW( Exception(_("Please install package 'lsof' first.") ) );
      }
      Exception err( str::Format("Executing 'lsof' failed (%1%).") % ret );
      err.remember( prog.execError() );
      ZYPP_THROW( err );
    }

    return _pimpl->createProcInfo( cachemap );
  }

  CheckAccessDeleted::size_type CheckAccessDeleted::Impl::createProcInfo(const std::map<pid_t,CacheEntry> &in)
  {
    std::ofstream debugFileOut;
    bool debugEnabled = false;
    if ( !_debugFile.empty() ) {
      debugFileOut.open( _debugFile.c_str() );
      debugEnabled =  debugFileOut.is_open();

      if ( !debugEnabled ) {
        ERR<<"Unable to open debug file: "<<_debugFile<<endl;
      }
    }

    _data.clear();
    for ( const auto &cached : in )
    {
      if (!debugEnabled)
        addDataIf( cached.second);
      else {
        std::vector<std::string> *mapPtr = nullptr;

        auto dbgInfo = debugMap.find(cached.first);
        if ( dbgInfo != debugMap.end() )
          mapPtr = &(dbgInfo->second);

        if( !addDataIf( cached.second, mapPtr ) )
          continue;

        for ( const std::string &dbgLine: dbgInfo->second ) {
          debugFileOut.write( dbgLine.c_str(), dbgLine.length() );
        }
      }
    }
    return _data.size();
  }

  bool CheckAccessDeleted::empty() const
  {
    return _pimpl->_data.empty();
  }

  CheckAccessDeleted::size_type CheckAccessDeleted::size() const
  {
    return _pimpl->_data.size();
  }

  CheckAccessDeleted::const_iterator CheckAccessDeleted::begin() const
  {
    return _pimpl->_data.begin();
  }

  CheckAccessDeleted::const_iterator CheckAccessDeleted::end() const
  {
    return _pimpl->_data.end();
  }

  void CheckAccessDeleted::setDebugOutputFile(const Pathname &filename_r)
  {
    _pimpl->_debugFile = filename_r;
  }

  std::string CheckAccessDeleted::findService( pid_t pid_r )
  {
    ProcInfo p;
    p.pid = str::numstring( pid_r );
    return p.service();
  }

  std::string CheckAccessDeleted::ProcInfo::service() const
  {
    static const str::regex rx( "[0-9]+:name=systemd:/system.slice/(.*/)?(.*).service$" );
    str::smatch what;
    std::string ret;
    iostr::simpleParseFile( InputStream( Pathname("/proc")/pid/"cgroup" ),
			    [&]( int num_r, std::string line_r )->bool
			    {
			      if ( str::regex_match( line_r, what, rx ) )
			      {
				ret = what[2];
				return false;	// stop after match
			      }
			      return true;
			    } );
    return ret;
  }

  /******************************************************************
  **
  **	FUNCTION NAME : operator<<
  **	FUNCTION TYPE : std::ostream &
  */
  std::ostream & operator<<( std::ostream & str, const CheckAccessDeleted & obj )
  {
    return dumpRange( str << "CheckAccessDeleted ",
                      obj.begin(),
                      obj.end() );
  }

   /******************************************************************
  **
  **	FUNCTION NAME : operator<<
  **	FUNCTION TYPE : std::ostream &
  */
  std::ostream & operator<<( std::ostream & str, const CheckAccessDeleted::ProcInfo & obj )
  {
    if ( obj.pid.empty() )
      return str << "<NoProc>";

    return dumpRangeLine( str << obj.command
                              << '<' << obj.pid
                              << '|' << obj.ppid
                              << '|' << obj.puid
                              << '|' << obj.login
                              << '>',
                          obj.files.begin(),
                          obj.files.end() );
  }

 /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
