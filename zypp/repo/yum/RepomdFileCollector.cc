/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "RepomdFileCollector.h"
#include <zypp/ZConfig.h>
#include <zypp/PathInfo.h>
#include <zypp/RepoInfo.h>
#include <solv/solvversion.h>

namespace zypp::env
{
  bool ZYPP_REPOMD_WITH_OTHER()
  {
    static bool val = [](){
      const char * env = getenv("ZYPP_REPOMD_WITH_OTHER");
      return( env && zypp::str::strToBool( env, true ) );
    }();
    return val;
  }

  bool ZYPP_REPOMD_WITH_FILELISTS()
  {
    static bool val = [](){
      const char * env = getenv("ZYPP_REPOMD_WITH_FILELISTS");
      return( env && zypp::str::strToBool( env, true ) );
    }();
    return val;
  }
}

namespace zypp::repo::yum
{

  namespace
  {
    inline OnMediaLocation loc_with_path_prefix( OnMediaLocation loc_r, const Pathname & prefix_r )
    {
      if ( ! prefix_r.empty() && prefix_r != "/" )
        loc_r.changeFilename( prefix_r / loc_r.filename() );
      return loc_r;
    }

    // search old repository file to run the delta algorithm on
    Pathname search_deltafile( const Pathname & dir, const Pathname & file )
    {
      Pathname deltafile;
      if ( ! PathInfo(dir).isDir() )
        return deltafile;

      // Strip the checksum preceding the file stem so we can look for an
      // old *-primary.xml which may contain some reusable blocks.
      std::string base { file.basename() };
      size_t hypoff = base.find( "-" );
      if ( hypoff != std::string::npos )
        base.replace( 0, hypoff + 1, "" );

      std::list<std::string> retlist;
      if ( ! filesystem::readdir( retlist, dir, false ) )
      {
        for ( const auto & fn : retlist )
        {
          if ( str::endsWith( fn, base ) )
            deltafile = fn;
        }
      }
      if ( !deltafile.empty() )
        return dir/deltafile;

      return deltafile;
    }
  } // namespace


  /**
   *  \class RepomdFileCollector
   *  \brief Helper filtering the files offered by a RepomdFileReader
   *
   *  Collected files from repomd.xml:
   *      File types:
   *          type        (plain)
   *          type_db     (sqlite, ignored by zypp)
   *          type_zck    (zchunk, preferred)
   *      Localized type:
   *          susedata.LOCALE
  */
  RepomdFileCollector::RepomdFileCollector( const Pathname &destDir_r )
    : _destDir { destDir_r }
  {
    addWantedLocale( ZConfig::instance().textLocale() );
    for ( const Locale & it : ZConfig::instance().repoRefreshLocales() )
      addWantedLocale( it );
  }

  RepomdFileCollector::~RepomdFileCollector()
  { }

  /** The callback invoked by the RepomdFileReader.
       * It's a pity, but in the presence of separate "type" and "type_zck" entries,
       * we have to scan the whole file before deciding what to download....
       */
  bool RepomdFileCollector::collect(const OnMediaLocation &loc_r, const std::string &typestr_r)
  {
    if ( str::endsWith( typestr_r, "_db" ) )
      return true;	// skip sqlitedb

    bool zchk { str::endsWith( typestr_r, "_zck" ) };
#if defined(LIBSOLVEXT_FEATURE_ZCHUNK_COMPRESSION)
    const std::string & basetype { zchk ? typestr_r.substr( 0, typestr_r.size()-4 ) : typestr_r };
#else
    if ( zchk )
      return true;	// skip zchunk if not supported by libsolv
    const std::string & basetype { typestr_r };
#endif

    // filter well known resource types
    if ( basetype == "other" && not env::ZYPP_REPOMD_WITH_OTHER() )
      return true;	// skip it

    if ( basetype == "filelists" && not env::ZYPP_REPOMD_WITH_FILELISTS() )
      return true;	// skip it

    // filter localized susedata
    if ( str::startsWith( basetype, "susedata." ) )
    {
      // susedata.LANG
      if ( ! wantLocale( Locale(basetype.c_str()+9) ) )
        return true;	// skip it
    }

    // may take it... (prefer zchnk)
    if ( zchk || !_wantedFiles.count( basetype ) )
      _wantedFiles[basetype] = loc_r;

    return true;
  }

  void RepomdFileCollector::finalize(const FinalizeCb &cb)
  {
    // schedule fileS for download
    for ( const auto & el : _wantedFiles ) {
      const OnMediaLocation & loc { el.second };
      const OnMediaLocation & loc_with_path { loc_with_path_prefix( loc, repoInfo().path() ) };
      cb( OnMediaLocation(loc_with_path).setDeltafile( search_deltafile( deltaDir()/"repodata", loc.filename() ) ) );
    }
  }

  bool RepomdFileCollector::wantLocale(const Locale &locale_r) const
  { return _wantedLocales.count( locale_r ); }

  void RepomdFileCollector::addWantedLocale(Locale locale_r)
  {
    while ( locale_r )
    {
      _wantedLocales.insert( locale_r );
      locale_r = locale_r.fallback();
    }
  }

}
