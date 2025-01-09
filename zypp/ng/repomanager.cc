/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "repomanager.h"
#include "zypp-core/ExternalProgram.h"

#include <solv/solvversion.h>

#include <zypp-core/AutoDispose.h>
#include <zypp-core/base/Regex.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/zyppng/pipelines/MTry>
#include <zypp-core/zyppng/pipelines/Transform>
#include <zypp-core/zyppng/ui/ProgressObserver>
#include <zypp-media/ng/providespec.h>
#include <zypp/HistoryLog.h>
#include <zypp/ZConfig.h>
#include <zypp/ZYppCallbacks.h>
#include <zypp/base/LogTools.h>
#include <zypp/ng/parser/RepoFileReader.h>
#include <zypp/ng/parser/servicefilereader.h>
#include <zypp/sat/Pool.h>
#include <zypp/zypp_detail/urlcredentialextractor_p.h>
#include <zypp/repo/ServiceType.h>
#include <zypp/ng/repo/pluginservices.h>

#include <zypp/ng/Context>
#include <zypp/ng/fusionpool.h>
#include <zypp/ng/reporthelper.h>
#include <zypp/ng/repo/refresh.h>
#include <zypp/ng/repo/workflows/repomanagerwf.h>
#include <zypp/ng/repo/workflows/serviceswf.h>


#include <fstream>
#include <utility>

#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::repomanager"

namespace zypp::zypp_readonly_hack {
  bool IGotIt(); // in readonly-mode
}


namespace zyppng
{
  namespace env
  {
    /** To trigger appdata refresh unconditionally */
    inline bool ZYPP_PLUGIN_APPDATA_FORCE_COLLECT()
    {
      const char * env = getenv("ZYPP_PLUGIN_APPDATA_FORCE_COLLECT");
      return( env && zypp::str::strToBool( env, true ) );
    }
  } // namespace env

  namespace {
    /** Delete \a cachePath_r subdirs not matching known aliases in \a repoEscAliases_r (must be sorted!)
     * \note bnc#891515: Auto-cleanup only zypp.conf default locations. Otherwise
     * we'd need some magic file to identify zypp cache directories. Without this
     * we may easily remove user data (zypper --pkg-cache-dir . download ...)
     */
    inline void cleanupNonRepoMetadataFolders( const zypp::Pathname & cachePath_r,
      const zypp::Pathname & defaultCachePath_r,
      const std::list<std::string> & repoEscAliases_r )
    {
      if ( cachePath_r != defaultCachePath_r )
        return;

      std::list<std::string> entries;
      if ( zypp::filesystem::readdir( entries, cachePath_r, false ) == 0 )
      {
        entries.sort();
        std::set<std::string> oldfiles;
        set_difference( entries.begin(), entries.end(), repoEscAliases_r.begin(), repoEscAliases_r.end(),
          std::inserter( oldfiles, oldfiles.end() ) );

        // bsc#1178966: Files or symlinks here have been created by the user
        // for whatever purpose. It's our cache, so we purge them now before
        // they may later conflict with directories we need.
        zypp::PathInfo pi;
        for ( const std::string & old : oldfiles )
        {
          if ( old == zypp::Repository::systemRepoAlias() )	// don't remove the @System solv file
            continue;
          pi( cachePath_r/old );
          if ( pi.isDir() )
            zypp::filesystem::recursive_rmdir( pi.path() );
          else
            zypp::filesystem::unlink( pi.path() );
        }
      }
    }
  } // namespace

  std::ostream & operator<<( std::ostream & str, zypp::RepoManagerFlags::RawMetadataRefreshPolicy obj )
  {
    switch ( obj ) {
#define OUTS(V) case zypp::RepoManagerFlags::V: str << #V; break
      OUTS( RefreshIfNeeded );
      OUTS( RefreshForced );
      OUTS( RefreshIfNeededIgnoreDelay );
#undef OUTS
    }
    return str;
  }

  std::ostream & operator<<( std::ostream & str, zypp::RepoManagerFlags::RefreshCheckStatus obj )
  {
    switch ( obj ) {
#define OUTS(V) case zypp::RepoManagerFlags::V: str << #V; break
      OUTS( REFRESH_NEEDED );
      OUTS( REPO_UP_TO_DATE );
      OUTS( REPO_CHECK_DELAYED );
#undef OUTS
    }
    return str;
  }

  std::ostream & operator<<( std::ostream & str, zypp::RepoManagerFlags::CacheBuildPolicy obj )
  {
    switch ( obj ) {
#define OUTS(V) case zypp::RepoManagerFlags::V: str << #V; break
      OUTS( BuildIfNeeded );
      OUTS( BuildForced );
#undef OUTS
    }
    return str;
  }


  std::string filenameFromAlias(const std::string &alias_r, const std::string &stem_r)
  {
    std::string filename( alias_r );
    // replace slashes with underscores
    zypp::str::replaceAll( filename, "/", "_" );

    filename = zypp::Pathname(filename).extend("."+stem_r).asString();
    MIL << "generating filename for " << stem_r << " [" << alias_r << "] : '" << filename << "'" << std::endl;
    return filename;
  }

  bool RepoCollector::collect(const RepoInfo &repo)
  {
    // skip repositories meant for other distros than specified
    if (!targetDistro.empty()
         && !repo.targetDistribution().empty()
         && repo.targetDistribution() != targetDistro)
    {
      MIL
        << "Skipping repository meant for '" << repo.targetDistribution()
        << "' distribution (current distro is '"
        << targetDistro << "')." << std::endl;

      return true;
    }

    repos.push_back(repo);
    return true;
  }

  expected<std::list<RepoInfo>> repositories_in_file( ContextBaseRef ctx, const zypp::Pathname &file)
  {
    try {
      MIL << "repo file: " << file << std::endl;
      RepoCollector collector;
      zyppng::parser::RepoFileReader parser( ctx, file, std::bind( &RepoCollector::collect, &collector, std::placeholders::_1 ) );
      return expected<std::list<RepoInfo>>::success( std::move(collector.repos) );
    } catch ( ... ) {
      return expected<std::list<RepoInfo>>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }
  }

  /**
     * \short List of RepoInfo's from a directory
     *
     * Goes trough every file ending with ".repo" in a directory and adds all
     * RepoInfo's contained in that file.
     *
     * \param zyppContext current context used for reports
     * \param dir pathname of the directory to read.
     */
  template <typename ZContextRef>
  std::list<RepoInfo> repositories_in_dir( ZContextRef zyppContext, ProgressObserverRef taskObserver, const zypp::Pathname &dir )
  {
    MIL << "directory " << dir << std::endl;
    std::list<RepoInfo> repos;
    bool nonroot( geteuid() != 0 );
    if ( nonroot && ! zypp::PathInfo(dir).userMayRX() )
    {
      JobReportHelper(zyppContext, taskObserver).warning( zypp::str::Format(_("Cannot read repo directory '%1%': Permission denied")) % dir );
    }
    else
    {
      std::list<zypp::Pathname> entries;
      if ( zypp::filesystem::readdir( entries, dir, false ) != 0 )
      {
        // TranslatorExplanation '%s' is a pathname
        ZYPP_THROW(zypp::Exception(zypp::str::form(_("Failed to read directory '%s'"), dir.c_str())));
      }

      zypp::str::regex allowedRepoExt("^\\.repo(_[0-9]+)?$");
      for ( std::list<zypp::Pathname>::const_iterator it = entries.begin(); it != entries.end(); ++it )
      {
        if ( zypp::str::regex_match(it->extension(), allowedRepoExt) )
        {
          if ( nonroot && ! zypp::PathInfo(*it).userMayR() )
          {
            JobReportHelper(zyppContext, taskObserver).warning( zypp::str::Format(_("Cannot read repo file '%1%': Permission denied")) % *it );
          }
          else
          {
            const std::list<RepoInfo> tmp( repositories_in_file( zyppContext, *it ).unwrap() );
            repos.insert( repos.end(), tmp.begin(), tmp.end() );
          }
        }
      }
    }
    return repos;
  }

  expected<void> assert_urls(const RepoInfo &info)
  {
    if ( info.baseUrlsEmpty() )
      return expected<void>::error( ZYPP_EXCPT_PTR ( zypp::repo::RepoNoUrlException( info ) ) );
    return expected<void>::success();
  }

  bool autoPruneInDir(const zypp::Pathname &path_r)
  { return not zypp::PathInfo(path_r/".no_auto_prune").isExist(); }


  template <typename ZyppContextType>
  RepoManager<ZyppContextType>::RepoManager( ZYPP_PRIVATE_CONSTR_ARG, Ref<ZyppContextType> zyppCtx, RepoManagerOptions opt )
    : _zyppContext( std::move(zyppCtx) )
    , _options( std::move(opt) )
    , _pluginRepoverification( _options.pluginsPath / "repoverification",
                               _options.rootDir)
  {

  }

  template <typename ZyppContextType>
  RepoManager<ZyppContextType>::~RepoManager()
  {
    // trigger appdata refresh if some repos change
    if ( ( _reposDirty || env::ZYPP_PLUGIN_APPDATA_FORCE_COLLECT() )
      && geteuid() == 0 && ( _options.rootDir.empty() || _options.rootDir == "/" ) )
    {
      try {
        std::list<zypp::Pathname> entries;
        zypp::filesystem::readdir( entries, _options.pluginsPath/"appdata", false );
        if ( ! entries.empty() )
        {
          zypp::ExternalProgram::Arguments cmd;
          cmd.push_back( "<" );		// discard stdin
          cmd.push_back( ">" );		// discard stdout
          cmd.push_back( "PROGRAM" );		// [2] - fix index below if changing!
          for ( const auto & rinfo : repos() )
          {
            if ( ! rinfo.second.enabled() )
              continue;
            cmd.push_back( "-R" );
            cmd.push_back( rinfo.second.alias() );
            cmd.push_back( "-t" );
            cmd.push_back( rinfo.second.type().asString() );
            cmd.push_back( "-p" );
            cmd.push_back( (rinfo.second.metadataPath()/rinfo.second.path()).asString() ); // bsc#1197684: path to the repodata/ directory inside the cache
          }

          for_( it, entries.begin(), entries.end() )
          {
            zypp::PathInfo pi( *it );
            //DBG << "/tmp/xx ->" << pi << endl;
            if ( pi.isFile() && pi.userMayRX() )
            {
              // trigger plugin
              cmd[2] = pi.asString();		// [2] - PROGRAM
              zypp::ExternalProgram prog( cmd, zypp::ExternalProgram::Stderr_To_Stdout );
            }
          }
        }
      }
      catch (...) {}	// no throw in dtor
    }
  }

  template<typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::initialize()
  {
    using namespace zyppng::operators;
    return
        init_knownServices()
        | and_then( [this](){ return init_knownRepositories(); } );
  }

  template<typename ZyppContextType>
  const RepoManagerOptions &RepoManager<ZyppContextType>::options() const
  {
    return _options;
  }

  template <typename ZyppContextType>
  expected<RepoStatus> RepoManager<ZyppContextType>::metadataStatus(const RepoInfo & info , const RepoManagerOptions &options)
  {
    try {
      using namespace zyppng::operators;

      // ATTENTION when making this pipeline async
      // consider moving it into a workflow object
      // this var is caputured by ref to modify it from
      // inside the pipeline, which would break.
      zypp::Pathname mediarootpath;

      return rawcache_path_for_repoinfo( options, info )
      | and_then( [&]( zypp::Pathname mrPath ) {
        mediarootpath = std::move(mrPath);
        return rawproductdata_path_for_repoinfo( options, info );
        })
      | and_then( [&]( zypp::Pathname productdatapath ) {
        zypp::repo::RepoType repokind = info.type();
        // If unknown, probe the local metadata
        if ( repokind == zypp::repo::RepoType::NONE )
          repokind = probeCache( productdatapath );

        // NOTE: The calling code expects an empty RepoStatus being returned
        // if the metadata cache is empty. So additional components like the
        // RepoInfos status are joined after the switch IFF the status is not
        // empty.huhu
        RepoStatus status;
        switch ( repokind.toEnum() )
        {
          case zypp::repo::RepoType::RPMMD_e :
            status = RepoStatus( productdatapath/"repodata/repomd.xml");
            if ( info.requireStatusWithMediaFile() )
              status = status && RepoStatus( mediarootpath/"media.1/media" );
            break;

          case zypp::repo::RepoType::YAST2_e :
            status = RepoStatus( productdatapath/"content" ) && RepoStatus( mediarootpath/"media.1/media" );
            break;

          case zypp::repo::RepoType::RPMPLAINDIR_e :
            // Dir status at last refresh. Plaindir uses the cookiefile as pseudo metadata index file.
            // It gets touched if the refresh check finds the data being up-to-date. That's why we use
            // the files mtime as timestamp (like the RepoStatus ctor in the other cases above).
            status = RepoStatus::fromCookieFileUseMtime( productdatapath/"cookie" );
            break;

          case zypp::repo::RepoType::NONE_e :
            // Return default RepoStatus in case of RepoType::NONE
            // indicating it should be created?
            // ZYPP_THROW(RepoUnknownTypeException());
            break;
        }

        if ( ! status.empty() )
          status = status && RepoStatus( info );

        return expected<RepoStatus>::success(status);
      });
    } catch (...) {
      return expected<RepoStatus>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }
  }

  template <typename ZyppContextType>
  expected<RepoStatus> RepoManager<ZyppContextType>::metadataStatus(const RepoInfo &info) const
  {
    return metadataStatus( info, _options );
  }

  template <typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::cleanMetadata(const RepoInfo &info, ProgressObserverRef myProgress )
  {
    try {

    ProgressObserver::setup( myProgress, _("Cleaning metadata"), 100 );
    ProgressObserver::start( myProgress );
    zypp::filesystem::recursive_rmdir( _zyppContext->config().geoipCachePath() );
    ProgressObserver::setCurrent ( myProgress, 50 );
    zypp::filesystem::recursive_rmdir( rawcache_path_for_repoinfo(_options, info).unwrap() );
    ProgressObserver::finish ( myProgress );

    } catch ( ... ) {
      ProgressObserver::finish ( myProgress, ProgressObserver::Error );
      return expected<void>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }
    return expected<void>::success();
  }

  template <typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::cleanPackages(const RepoInfo &info, ProgressObserverRef myProgress, bool isAutoClean  )
  {
    try {
    ProgressObserver::setup( myProgress, _("Cleaning packages"), 100 );
    ProgressObserver::start( myProgress );

    // bsc#1204956: Tweak to prevent auto pruning package caches
    zypp::Pathname rpc { packagescache_path_for_repoinfo(_options, info).unwrap() };
    if ( not isAutoClean || autoPruneInDir( rpc.dirname() ) )
      zypp::filesystem::recursive_rmdir( rpc );

    ProgressObserver::finish ( myProgress );

    } catch (...) {
      ProgressObserver::finish ( myProgress, ProgressObserver::Error );
      return expected<void>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }

    return expected<void>::success();
  }

  /** Probe Metadata in a local cache directory
   *
   * \note Metadata in local cache directories must not be probed using \ref probe as
   * a cache path must not be rewritten (bnc#946129)
   */
  template <typename ZyppContextType>
  zypp::repo::RepoType RepoManager<ZyppContextType>::probeCache( const zypp::Pathname & path_r )
  {
    MIL << "going to probe the cached repo at " << path_r << std::endl;

    zypp::repo::RepoType ret = zypp::repo::RepoType::NONE;

    if ( zypp::PathInfo(path_r/"/repodata/repomd.xml").isFile() )
    { ret = zypp::repo::RepoType::RPMMD; }
    else if ( zypp::PathInfo(path_r/"/content").isFile() )
    { ret = zypp::repo::RepoType::YAST2; }
    else if ( zypp::PathInfo(path_r/"/cookie").isFile() )
    { ret = zypp::repo::RepoType::RPMPLAINDIR; }

    MIL << "Probed cached type " << ret << " at " << path_r << std::endl;
    return ret;
  }

  template <typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::cleanCacheDirGarbage( ProgressObserverRef myProgress )
  {
    try {
      MIL << "Going to clean up garbage in cache dirs" << std::endl;

      std::list<zypp::Pathname> cachedirs;
      cachedirs.push_back(_options.repoRawCachePath);
      cachedirs.push_back(_options.repoPackagesCachePath);
      cachedirs.push_back(_options.repoSolvCachePath);

      ProgressObserver::setup( myProgress, _("Cleaning up cache dirs"), cachedirs.size() );
      ProgressObserver::start( myProgress );

      for( const auto &dir : cachedirs )
      {
        // increase progress on end of every iteration
        zypp_defer {
          ProgressObserver::increase( myProgress );
        };

        if ( zypp::PathInfo(dir).isExist() )
        {
          std::list<zypp::Pathname> entries;
          if ( zypp::filesystem::readdir( entries, dir, false ) != 0 )
            // TranslatorExplanation '%s' is a pathname
            ZYPP_THROW(zypp::Exception(zypp::str::form(_("Failed to read directory '%s'"), dir.c_str())));

          if ( !entries.size() )
            continue;

          auto dirProgress = ProgressObserver::makeSubTask( myProgress, 1.0, zypp::str::Format( _("Cleaning up directory: %1%") ) % dir, entries.size() );
          for( const auto &subdir : entries )
          {
            // if it does not belong known repo, make it disappear
            bool found = false;
            for_( r, repoBegin(), repoEnd() )
              if ( subdir.basename() == r->second.escaped_alias() )
            { found = true; break; }

            if ( ! found && ( zypp::Date::now()-zypp::PathInfo(subdir).mtime() > zypp::Date::day ) )
              zypp::filesystem::recursive_rmdir( subdir );

            ProgressObserver::increase( dirProgress );
          }
          ProgressObserver::finish( dirProgress );
        }
      }
    } catch (...) {
      // will finish all subprogress children
      ProgressObserver::finish ( myProgress, ProgressObserver::Error );
      return expected<void>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }
    ProgressObserver::finish ( myProgress );
    return expected<void>::success();
  }

  template <typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::cleanCache(const RepoInfo &info, ProgressObserverRef myProgress )
  {
    try {
    ProgressObserver::setup( myProgress, _("Cleaning cache"), 100 );
    ProgressObserver::start( myProgress );

    MIL << "Removing raw metadata cache for " << info.alias() << std::endl;
    zypp::filesystem::recursive_rmdir(solv_path_for_repoinfo(_options, info).unwrap());

    ProgressObserver::finish( myProgress );
    return expected<void>::success();

    } catch (...) {
      // will finish all subprogress children
      ProgressObserver::finish ( myProgress, ProgressObserver::Error );
      return expected<void>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }
  }

  template <typename ZyppContextType>
  typename RepoManager<ZyppContextType>::template MaybeAsyncRef<expected<void>> RepoManager<ZyppContextType>::loadFromCache( FusionPoolRef<ContextType> fPool, RepoInfo info, ProgressObserverRef myProgress )
  {
    using namespace zyppng::operators;

    if ( !info.context() || info.context() != zyppContext() ) {
      return makeReadyResult(expected<void>::error( ZYPP_EXCPT_PTR( zypp::Exception("None or different context in RepoInfo is not supported.")) ));
    }

    return zyppng::mtry( [this, fPool, info, myProgress](){
      ProgressObserver::setup( myProgress, _("Loading from cache"), 3 );
      ProgressObserver::start( myProgress );

      assert_alias(info).unwrap();
      zypp::Pathname solvfile = solv_path_for_repoinfo(_options, info).unwrap() / "solv";

      if ( ! zypp::PathInfo(solvfile).isExist() )
        ZYPP_THROW(zypp::repo::RepoNotCachedException(info));

      fPool->satPool().reposErase( info.alias() );

      ProgressObserver::increase ( myProgress );

      zypp::Repository repo = fPool->satPool().addRepoSolv( solvfile, info );

      ProgressObserver::increase ( myProgress );

      // test toolversion in order to rebuild solv file in case
      // it was written by a different libsolv-tool parser.
      const std::string & toolversion( zypp::sat::LookupRepoAttr( zypp::sat::SolvAttr::repositoryToolVersion, repo ).begin().asString() );
      if ( toolversion != LIBSOLV_TOOLVERSION ) {
        repo.eraseFromPool();
        ZYPP_THROW(zypp::Exception(zypp::str::Str() << "Solv-file was created by '"<<toolversion<<"'-parser (want "<<LIBSOLV_TOOLVERSION<<")."));
      }
    })
    | or_else( [this, fPool, info, myProgress]( std::exception_ptr exp ) {
      ZYPP_CAUGHT( exp );
      MIL << "Try to handle exception by rebuilding the solv-file" << std::endl;
      return cleanCache( info, ProgressObserver::makeSubTask( myProgress ) )
        | and_then([this, info =  info, myProgress] () mutable {
          return buildCache ( info, zypp::RepoManagerFlags::BuildIfNeeded, ProgressObserver::makeSubTask( myProgress ) );
        })
        | and_then( mtry([this, fPool]( RepoInfo info ){
          fPool->satPool().addRepoSolv( solv_path_for_repoinfo(_options, info).unwrap() / "solv", info );
        }));
    })
    | and_then([myProgress]{
      ProgressObserver::finish ( myProgress );
      return expected<void>::success();
    })
    | or_else([myProgress]( auto ex ){
      ProgressObserver::finish ( myProgress, ProgressObserver::Error );
      return expected<void>::error(ex);
    })
    ;
  }

  template <typename ZyppContextType>
  expected<RepoInfo> RepoManager<ZyppContextType>::addProbedRepository( RepoInfo info, zypp::repo::RepoType probedType )
  {
    try {
      auto tosave = info;

      if ( repos().find(tosave.alias()) != repos().end() )
        return expected<RepoInfo>::error( ZYPP_EXCPT_PTR(zypp::repo::RepoAlreadyExistsException(tosave)) );

      // assert the directory exists
      zypp::filesystem::assert_dir(_options.knownReposPath);

      zypp::Pathname repofile = generateNonExistingName(
        _options.knownReposPath, generateFilename(tosave));
      // now we have a filename that does not exists
      MIL << "Saving repo in " << repofile << std::endl;

      std::ofstream file(repofile.c_str());
      if (!file)
      {
        // TranslatorExplanation '%s' is a filename
        ZYPP_THROW( zypp::Exception(zypp::str::form( _("Can't open file '%s' for writing."), repofile.c_str() )));
      }

      tosave.dumpAsIniOn(file);
      tosave.setFilepath(repofile);
      tosave.setMetadataPath( rawcache_path_for_repoinfo( _options, tosave ).unwrap() );
      tosave.setPackagesPath( packagescache_path_for_repoinfo( _options, tosave ).unwrap() );
      reposManip().insert( std::make_pair( tosave.alias(), tosave) );

      // check for credentials in Urls
      zypp::UrlCredentialExtractor( _zyppContext ).collect( tosave.baseUrls() );

      zypp::HistoryLog(_options.rootDir).addRepository(tosave);

      // return the new repoinfo
      return expected<RepoInfo>::success( tosave );

    } catch (...) {
      return expected<RepoInfo>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }
  }

  template <typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::removeRepository( RepoInfo & info,  ProgressObserverRef myProgress )
  {
    try {
    ProgressObserver::setup( myProgress, zypp::str::form(_("Removing repository '%s'"), info.label().c_str()), 1 );
    ProgressObserver::start( myProgress );

    prepareRepoInfo(info).unwrap();

    MIL << "Going to delete repo " << info.alias() << std::endl;

    for( const auto &repo : repos() )
    {
      // they can be the same only if the provided is empty, that means
      // the provided repo has no alias
      // then skip
      if ( (!info.alias().empty()) && ( info.alias() != repo.first ) )
        continue;

      // TODO match by url

      // we have a matching repository, now we need to know
      // where it does come from.
      RepoInfo todelete = repo.second;
      if (todelete.filepath().empty())
      {
        ZYPP_THROW(zypp::repo::RepoException( todelete, _("Can't figure out where the repo is stored.") ));
      }
      else
      {
        // figure how many repos are there in the file:
        std::list<RepoInfo> filerepos = repositories_in_file( _zyppContext, todelete.filepath()).unwrap();
        std::for_each( filerepos.begin (), filerepos.end(), [this]( RepoInfo &info ){ prepareRepoInfo(info).unwrap(); });

        if ( filerepos.size() == 0	// bsc#984494: file may have already been deleted
             ||(filerepos.size() == 1 && filerepos.front().alias() == todelete.alias() ) )
        {
          // easy: file does not exist, contains no or only the repo to delete: delete the file
          int ret = zypp::filesystem::unlink( todelete.filepath() );
          if ( ! ( ret == 0 || ret == ENOENT ) )
          {
            // TranslatorExplanation '%s' is a filename
            ZYPP_THROW(zypp::repo::RepoException( todelete, zypp::str::form( _("Can't delete '%s'"), todelete.filepath().c_str() )));
          }
          MIL << todelete.alias() << " successfully deleted." << std::endl;
        }
        else
        {
          // there are more repos in the same file
          // write them back except the deleted one.
          //TmpFile tmp;
          //std::ofstream file(tmp.path().c_str());

          // assert the directory exists
          zypp::filesystem::assert_dir(todelete.filepath().dirname());

          std::ofstream file(todelete.filepath().c_str());
          if (!file)
          {
            // TranslatorExplanation '%s' is a filename
            ZYPP_THROW( zypp::Exception(zypp::str::form( _("Can't open file '%s' for writing."), todelete.filepath().c_str() )));
          }
          for ( std::list<RepoInfo>::const_iterator fit = filerepos.begin();
                fit != filerepos.end();
                ++fit )
          {
            if ( (*fit).alias() != todelete.alias() )
              (*fit).dumpAsIniOn(file);
          }
        }

        // now delete it from cache
        if ( isCached(todelete) )
          cleanCache( todelete, ProgressObserver::makeSubTask( myProgress, 0.2 )).unwrap();
        // now delete metadata (#301037)
        cleanMetadata( todelete, ProgressObserver::makeSubTask( myProgress, 0.4 )).unwrap();
        cleanPackages( todelete, ProgressObserver::makeSubTask( myProgress, 0.4 ), true/*isAutoClean*/ ).unwrap();
        reposManip().erase(todelete.alias());
        MIL << todelete.alias() << " successfully deleted." << std::endl;
        zypp::HistoryLog(_options.rootDir).removeRepository(todelete);

        ProgressObserver::finish(myProgress);
        return expected<void>::success();
      } // else filepath is empty
    }
    // should not be reached on a sucess workflow
    ZYPP_THROW(zypp::repo::RepoNotFoundException(info));
    } catch (...) {
      ProgressObserver::finish( myProgress, ProgressObserver::Error );
      return expected<void>::error( std::current_exception () );
    }
  }

  template <typename ZyppContextType>
  expected<RepoInfo> RepoManager<ZyppContextType>::modifyRepository(const std::string & alias, RepoInfo newinfo_r, ProgressObserverRef myProgress )
  {
    try {

    ProgressObserver::setup( myProgress, _("Modifying repository"), 5 );
    ProgressObserver::start( myProgress );

    RepoInfo toedit = getRepositoryInfo(alias).unwrap();

    RepoInfo newinfo( newinfo_r ); // need writable copy to upadte housekeeping data
    prepareRepoInfo (newinfo).unwrap ();

    // check if the new alias already exists when renaming the repo
    if ( alias != newinfo.alias() && hasRepo( newinfo.alias() ) )
    {
      ZYPP_THROW(zypp::repo::RepoAlreadyExistsException(newinfo));
    }

    if (toedit.filepath().empty())
    {
      ZYPP_THROW(zypp::repo::RepoException( toedit, _("Can't figure out where the repo is stored.") ));
    }
    else
    {
      ProgressObserver::increase( myProgress );
      // figure how many repos are there in the file:
      std::list<RepoInfo> filerepos = repositories_in_file( _zyppContext, toedit.filepath()).unwrap();
      std::for_each( filerepos.begin (), filerepos.end(), [this]( RepoInfo &info ){ prepareRepoInfo(info).unwrap(); });

      // there are more repos in the same file
      // write them back except the deleted one.
      //TmpFile tmp;
      //std::ofstream file(tmp.path().c_str());

      // assert the directory exists
      zypp::filesystem::assert_dir(toedit.filepath().dirname());

      std::ofstream file(toedit.filepath().c_str());
      if (!file)
      {
        // TranslatorExplanation '%s' is a filename
        ZYPP_THROW( zypp::Exception(zypp::str::form( _("Can't open file '%s' for writing."), toedit.filepath().c_str() )));
      }
      for ( std::list<RepoInfo>::const_iterator fit = filerepos.begin();
            fit != filerepos.end();
            ++fit )
      {
        // if the alias is different, dump the original
        // if it is the same, dump the provided one
        if ( (*fit).alias() != toedit.alias() )
          (*fit).dumpAsIniOn(file);
        else
          newinfo.dumpAsIniOn(file);
      }

      ProgressObserver::increase( myProgress );

      if ( toedit.enabled() && !newinfo.enabled() )
      {
        // On the fly remove solv.idx files for bash completion if a repo gets disabled.
        const zypp::Pathname solvidx = solv_path_for_repoinfo(_options, newinfo).unwrap()/"solv.idx";
        if ( zypp::PathInfo(solvidx).isExist() )
          zypp::filesystem::unlink( solvidx );
      }

      newinfo.setFilepath(toedit.filepath());
      newinfo.setMetadataPath( rawcache_path_for_repoinfo( _options, newinfo ).unwrap() );
      newinfo.setPackagesPath( packagescache_path_for_repoinfo( _options, newinfo ).unwrap() );

      ProgressObserver::increase( myProgress );

      reposManip().erase(toedit.alias());
      reposManip().insert(std::make_pair(newinfo.alias(), newinfo));

      ProgressObserver::increase( myProgress );

      // check for credentials in Urls
      zypp::UrlCredentialExtractor( _zyppContext ).collect( newinfo.baseUrls() );
      zypp::HistoryLog(_options.rootDir).modifyRepository(toedit, newinfo);
      MIL << "repo " << alias << " modified" << std::endl;

      ProgressObserver::finish ( myProgress );

      return expected<RepoInfo>::success(newinfo);
    }

    } catch ( ... ) {
      ProgressObserver::finish ( myProgress, ProgressObserver::Error );
      return expected<RepoInfo>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }
  }

  template <typename ZyppContextType>
  expected<RepoInfo> RepoManager<ZyppContextType>::getRepositoryInfo( const std::string & alias  )
  {
    try {
    RepoConstIterator it( findAlias( alias, repos() ) );
    if ( it != repos().end() )
      return make_expected_success(it->second);
    RepoInfo info( _zyppContext );
    info.setAlias( alias );
    ZYPP_THROW( zypp::repo::RepoNotFoundException(info) );
    } catch ( ... ) {
      return expected<RepoInfo>::error( std::current_exception () );
    }
  }


  template <typename ZyppContextType>
  expected<RepoInfo> RepoManager<ZyppContextType>::getRepositoryInfo( const zypp::Url & url, const zypp::url::ViewOption & urlview )
  {
    try {

    for_( it, repoBegin(), repoEnd() )
    {
      for_( urlit, it->second.baseUrlsBegin(), it->second.baseUrlsEnd() )
      {
        if ( (*urlit).asString(urlview) == url.asString(urlview) )
          return make_expected_success(it->second);
      }
    }
    RepoInfo info( _zyppContext );
    info.setBaseUrl( url );
    ZYPP_THROW( zypp::repo::RepoNotFoundException(info) );

    } catch ( ... ) {
      return expected<RepoInfo>::error( std::current_exception () );
    }
  }

  template<typename ZyppContextType>
  typename RepoManager<ZyppContextType>::template MaybeAsyncRef<expected<std::pair<RepoInfo,typename RepoManager<ZyppContextType>::RefreshCheckStatus>>> RepoManager<ZyppContextType>::checkIfToRefreshMetadata( RepoInfo info, const zypp::Url &url, RawMetadataRefreshPolicy policy)
  {
    using namespace zyppng::operators;
    return RepoManagerWorkflow::refreshGeoIPData( _zyppContext, {url} )
      | [this, info](auto) { return prepareRefreshContext(info); }
      | and_then( [this, url, policy]( zyppng::repo::RefreshContextRef<ZyppContextType> &&refCtx ) {
        refCtx->setPolicy ( static_cast<zyppng::repo::RawMetadataRefreshPolicy>( policy ) );
        return refCtx->engageLock( )
            | and_then( [this, url, refCtx ]( zypp::Deferred lockRef ) {
              return _zyppContext->provider()->prepareMedia( url, zyppng::ProvideMediaSpec(_zyppContext) )
                  | and_then( [ refCtx ]( auto mediaHandle ) mutable { return zyppng::RepoManagerWorkflow::checkIfToRefreshMetadata ( std::move(refCtx), std::move(mediaHandle), nullptr ); } )
                  | and_then( [ refCtx, lockRef ]( auto checkStatus ){ return make_expected_success (std::make_pair( refCtx->repoInfo(), checkStatus ) ); });
            });
      });
  }

  template<typename ZyppContextType>
  typename RepoManager<ZyppContextType>::template MaybeAsyncRef<expected<RepoInfo>> RepoManager<ZyppContextType>::refreshMetadata( RepoInfo info, RawMetadataRefreshPolicy policy, ProgressObserverRef myProgress )
  {
    using namespace zyppng::operators;
    // helper callback in case the repo type changes on the remote
    // do NOT capture by reference here, since this is possibly executed async
    const auto &updateProbedType = [this, info = info]( zypp::repo::RepoType repokind ) {
      // update probed type only for repos in system
      for( const auto &repoIter : repos() ) {
        if ( info.alias() == repoIter.first )
        {
          RepoInfo modifiedrepo = repoIter.second;
          modifiedrepo.setType( repokind );
          // don't modify .repo in refresh.
          // modifyRepository( info.alias(), modifiedrepo ); m
          break;
        }
      }
    };

      // make sure geoIP data is up 2 date, but ignore errors
    return RepoManagerWorkflow::refreshGeoIPData( _zyppContext, info.baseUrls() )
      | [this, info = info](auto) { return prepareRefreshContext(info); }
      | and_then( [policy, myProgress, cb = updateProbedType]( repo::RefreshContextRef<ZyppContextType> refCtx ) {
        refCtx->setPolicy( static_cast<repo::RawMetadataRefreshPolicy>( policy ) );
        // in case probe detects a different repokind, update our internal repos
        refCtx->connectFunc( &repo::RefreshContext<ZyppContextType>::sigProbedTypeChanged, cb );

        return zyppng::RepoManagerWorkflow::refreshMetadata ( std::move(refCtx), myProgress );
      })
      | and_then([rMgr = shared_this<RepoManager<ZyppContextType>>()]( repo::RefreshContextRef<ZyppContextType> ctx ) {

        if ( ! isTmpRepo( ctx->repoInfo() ) )
          rMgr->reposManip();	// remember to trigger appdata refresh

        return expected<RepoInfo>::success ( ctx->repoInfo() );
    });
  }

  template<typename ZyppContextType>
  typename RepoManager<ZyppContextType>::template MaybeAsyncRef<std::vector<std::pair<RepoInfo, expected<void> > > > RepoManager<ZyppContextType>::refreshMetadata( std::vector<RepoInfo> infos, RawMetadataRefreshPolicy policy, ProgressObserverRef myProgress )
  {
    using namespace zyppng::operators;

    ProgressObserver::setup( myProgress, "Refreshing repositories" , 1 );

    return std::move(infos)
        | transform( [this, policy, myProgress]( const RepoInfo &info ) {

        auto subProgress = ProgressObserver::makeSubTask( myProgress, 1.0, zypp::str::Str() << _("Refreshing Repository: ") << info.alias(), 3 );
        ProgressObserver::start( subProgress );

        // helper callback in case the repo type changes on the remote
        // do NOT capture by reference here, since this is possibly executed async
        const auto &updateProbedType = [this, info = info]( zypp::repo::RepoType repokind ) {
          // update probed type only for repos in system
          for( const auto &repoIter : repos() ) {
            if ( info.alias() == repoIter.first )
            {
              RepoInfo modifiedrepo = repoIter.second;
              modifiedrepo.setType( repokind );
              // don't modify .repo in refresh.
              // modifyRepository( info.alias(), modifiedrepo );
              break;
            }
          }
        };

        auto sharedThis = shared_this<RepoManager<ZyppContextType>>();

        return
          // make sure geoIP data is up 2 date, but ignore errors
          RepoManagerWorkflow::refreshGeoIPData( _zyppContext, info.baseUrls() )
          | [sharedThis, info = info](auto) { return sharedThis->prepareRefreshContext(info); }
          | inspect( incProgress( subProgress ) )
          | and_then( [policy, subProgress, cb = updateProbedType]( repo::RefreshContextRef<ZyppContextType> refCtx ) {
            refCtx->setPolicy( static_cast<repo::RawMetadataRefreshPolicy>( policy ) );
            // in case probe detects a different repokind, update our internal repos
            refCtx->connectFunc( &repo::RefreshContext<ZyppContextType>::sigProbedTypeChanged, cb );

            return zyppng::RepoManagerWorkflow::refreshMetadata ( std::move(refCtx), ProgressObserver::makeSubTask( subProgress ) );
          })
          | inspect( incProgress( subProgress ) )
          | and_then([subProgress]( repo::RefreshContextRef<ZyppContextType> ctx ) {

            if ( ! isTmpRepo( ctx->repoInfo() ) )
              ctx->repoManager()->reposManip();	// remember to trigger appdata refresh

            return zyppng::RepoManagerWorkflow::buildCache ( std::move(ctx), CacheBuildPolicy::BuildIfNeeded, ProgressObserver::makeSubTask( subProgress ) );
          })
          | inspect( incProgress( subProgress ) )
          | [ info = info, subProgress ]( expected<repo::RefreshContextRef<ZyppContextType>> result ) {
            if ( result ) {
              ProgressObserver::finish( subProgress, ProgressObserver::Success );
              RepoInfo info = result.get()->repoInfo();
              info.setName( info.name() + " refreshed" );
              return std::make_pair(info, expected<void>::success() );
            } else {
              ProgressObserver::finish( subProgress, ProgressObserver::Error );
              return std::make_pair(info, expected<void>::error( result.error() ) );
            }
          };
      }
      | [myProgress]( auto res ) {
        ProgressObserver::finish( myProgress, ProgressObserver::Success );
        return res;
      }
    );
  }

  /** Probe the metadata type of a repository located at \c url.
   * Urls here may be rewritten by \ref MediaSetAccess or \ref zyppng::Provide to reflect the correct media number.
   *
   * \note Metadata in local cache directories must be probed using \ref probeCache as
   * a cache path must not be rewritten (bnc#946129)
   */
  template<typename ZyppContextType>
  typename RepoManager<ZyppContextType>::template MaybeAsyncRef<expected<zypp::repo::RepoType>> RepoManager<ZyppContextType>::probe(const zypp::Url &url, const zypp::Pathname &path) const
  {
    using namespace zyppng::operators;
    return RepoManagerWorkflow::refreshGeoIPData( _zyppContext, {url} )
      | [this, url=url](auto) { return _zyppContext->provider()->prepareMedia( url, zyppng::ProvideMediaSpec(_zyppContext) ); }
      | and_then( [this, path = path]( auto mediaHandle ) {
        return RepoManagerWorkflow::probeRepoType( _zyppContext, std::forward<decltype(mediaHandle)>(mediaHandle), path );
    });
  }

  template<typename ZyppContextType>
  typename RepoManager<ZyppContextType>::template MaybeAsyncRef<expected<RepoInfo>> RepoManager<ZyppContextType>::buildCache( RepoInfo info, CacheBuildPolicy policy, ProgressObserverRef myProgress )
  {
    using namespace zyppng::operators;

    return cloneAndPrepare ( info )
      | and_then( [this ]( auto info ) { return zyppng::repo::RefreshContext<ZyppContextType>::create( shared_this<RepoManager<ZyppContextType>>(), info ); } )
      | and_then( [policy, myProgress]( repo::RefreshContextRef<ZyppContextType> refCtx ) {
        return zyppng::RepoManagerWorkflow::buildCache ( std::move(refCtx), policy, myProgress );
      })
      | and_then([]( zyppng::repo::RefreshContextRef<ZyppContextType> ctx ){ return make_expected_success(ctx->repoInfo());})
    ;
  }

  template<typename ZyppContextType>
  typename RepoManager<ZyppContextType>::template MaybeAsyncRef<expected<RepoInfo>> RepoManager<ZyppContextType>::addRepository( RepoInfo info, ProgressObserverRef myProgress )
  {
    using namespace zyppng::operators;
    return cloneAndPrepare (info)
      | and_then( [this, myProgress]( RepoInfo info ){
        return RepoManagerWorkflow::addRepository( shared_this<RepoManager<ZyppContextType>>(), info, std::move(myProgress) );
      });
  }

  template<typename ZyppContextType>
  typename RepoManager<ZyppContextType>::template MaybeAsyncRef<expected<void>> RepoManager<ZyppContextType>::addRepositories( const zypp::Url &url, ProgressObserverRef myProgress )
  {
    using namespace zyppng::operators;
    return RepoManagerWorkflow::addRepositories( shared_this<RepoManager<ZyppContextType>>(), url, std::move(myProgress));
  }

  template <typename ZyppContextType>
  typename RepoManager<ZyppContextType>::template MaybeAsyncRef<expected<zypp::repo::ServiceType>> RepoManager<ZyppContextType>::probeService( const zypp::Url & url ) const
  {
    return RepoServicesWorkflow::probeServiceType ( _zyppContext, url );
  }

  template <typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::addService( const ServiceInfo & service )
  {
    try {

    assert_alias( service ).unwrap();

    // check if service already exists
    if ( hasService( service.alias() ) )
      ZYPP_THROW( zypp::repo::ServiceAlreadyExistsException( service ) );

    // Writable ServiceInfo is needed to save the location
    // of the .service file. Finaly insert into the service list.
    ServiceInfo toSave( service );
    saveService( toSave ).unwrap();
    _services.insert( std::make_pair( toSave.alias(), toSave ) );

    // check for credentials in Url
    zypp::UrlCredentialExtractor( _zyppContext ).collect( toSave.url() );

    MIL << "added service " << toSave.alias() << std::endl;

    } catch ( ... ) {
      return expected<void>::error( std::current_exception () );
    }

    return expected<void>::success();
  }

  template<typename ZyppContextType>
  typename RepoManager<ZyppContextType>::template MaybeAsyncRef<expected<void>> RepoManager<ZyppContextType>::refreshService( const std::string &alias, const RefreshServiceOptions &options_r )
  {
    const auto &serviceOpt = getService(alias);
    if( !serviceOpt )
    {
      ZYPP_THROW(zypp::repo::ServiceException( _("Can't find service alias.") ));
    }
    return RepoServicesWorkflow::refreshService( shared_this<RepoManager<ZyppContextType>>(), *serviceOpt, options_r );
  }

  /*!
   * \todo ignore ServicePluginInformalException in calling code
   */
  template<typename ZyppContextType>
  typename RepoManager<ZyppContextType>::template MaybeAsyncRef<expected<void>> RepoManager<ZyppContextType>::refreshServices(const RefreshServiceOptions &options_r)
  {
    using namespace zyppng::operators;

    // copy the set of services since refreshService
    // can eventually invalidate the iterator
    // save it into a vector, transform needs a container with push_back support
    std::vector<ServiceInfo> servicesVec;
    std::transform( serviceBegin(), serviceEnd(), std::back_inserter(servicesVec), []( const auto &pair ){ return pair.second;} );

    return std::move(servicesVec)
      | transform( [options_r, this]( ServiceInfo i ){ return RepoServicesWorkflow::refreshService( shared_this<RepoManager<ZyppContextType>>(), i, options_r ); } )
      | join()
      | collect();
  }

  ////////////////////////////////////////////////////////////////////////////

  template <typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::removeService( const std::string & alias )
  {
    try {
    MIL << "Going to delete service " << alias << std::endl;

    const auto &serviceOpt = getService(alias);
    if( !serviceOpt )
    {
      ZYPP_THROW(zypp::repo::ServiceException( _("Can't find service alias.") ));
    }

    const ServiceInfo & service = *serviceOpt;

    zypp::Pathname location = service.filepath();
    if( location.empty() )
    {
      ZYPP_THROW(zypp::repo::ServiceException( service, _("Can't figure out where the service is stored.") ));
    }

    ServiceMap tmpMap;
    parser::ServiceFileReader( _zyppContext, location, ServiceCollector(tmpMap) );

    // only one service definition in the file
    if ( tmpMap.size() == 1 )
    {
      if ( zypp::filesystem::unlink(location) != 0 )
      {
        // TranslatorExplanation '%s' is a filename
        ZYPP_THROW(zypp::repo::ServiceException( service, zypp::str::form( _("Can't delete '%s'"), location.c_str() ) ));
      }
      MIL << alias << " successfully deleted." << std::endl;
    }
    else
    {
      zypp::filesystem::assert_dir(location.dirname());

      std::ofstream file(location.c_str());
      if( !file )
      {
        // TranslatorExplanation '%s' is a filename
        ZYPP_THROW( zypp::Exception(zypp::str::form( _("Can't open file '%s' for writing."), location.c_str() )));
      }

      for_(it, tmpMap.begin(), tmpMap.end())
      {
        if( it->first != alias )
          it->second.dumpAsIniOn(file);
      }

      MIL << alias << " successfully deleted from file " << location <<  std::endl;
    }

    // now remove all repositories added by this service
    RepoCollector rcollector;
    getRepositoriesInService( alias,
      boost::make_function_output_iterator( std::bind( &RepoCollector::collect, &rcollector, std::placeholders::_1 ) ) );
    // cannot do this directly in getRepositoriesInService - would invalidate iterators
    for_(rit, rcollector.repos.begin(), rcollector.repos.end())
      removeRepository(*rit).unwrap();

    return expected<void>::success();

    } catch ( ... ) {
      return expected<void>::error( std::current_exception () );
    }
  }

  template <typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::modifyService( const std::string & oldAlias, ServiceInfo newService )
  {
    try {

    MIL << "Going to modify service " << oldAlias << std::endl;
    if ( newService.type() == zypp::repo::ServiceType::PLUGIN )
    {
      ZYPP_THROW(zypp::repo::ServicePluginImmutableException( newService ));
    }

    const auto &oldServiceOpt = getService(oldAlias);
    if( !oldServiceOpt )
    {
      ZYPP_THROW(zypp::repo::ServiceException( _("Can't find service alias.") ));
    }

    const ServiceInfo &oldService = *oldServiceOpt;

    zypp::Pathname location = oldService.filepath();
    if( location.empty() )
    {
      ZYPP_THROW(zypp::repo::ServiceException( oldService, _("Can't figure out where the service is stored.") ));
    }

    // remember: there may multiple services being defined in one file:
    ServiceMap tmpMap;
    parser::ServiceFileReader( _zyppContext, location, ServiceCollector(tmpMap) );

    zypp::filesystem::assert_dir(location.dirname());
    std::ofstream file(location.c_str());
    for_(it, tmpMap.begin(), tmpMap.end())
    {
      if( it->first != oldAlias )
        it->second.dumpAsIniOn(file);
    }
    newService.dumpAsIniOn(file);
    file.close();
    newService.setFilepath(location);

    _services.erase  ( oldAlias );
    _services.insert ( std::make_pair( newService.alias(), newService) );
    // check for credentials in Urls
    zypp::UrlCredentialExtractor( _zyppContext ).collect( newService.url() );


    // changed properties affecting also repositories
    if ( oldAlias != newService.alias()			// changed alias
         || oldService.enabled() != newService.enabled() )	// changed enabled status
    {
      std::vector<RepoInfo> toModify;
      getRepositoriesInService(oldAlias, std::back_inserter(toModify));
      for_( it, toModify.begin(), toModify.end() )
      {
        if ( oldService.enabled() != newService.enabled() )
        {
          if ( newService.enabled() )
          {
            // reset to last refreshs state
            const auto & last = newService.repoStates().find( it->alias() );
            if ( last != newService.repoStates().end() )
              it->setEnabled( last->second.enabled );
          }
          else
            it->setEnabled( false );
        }

        if ( oldAlias != newService.alias() )
          it->setService(newService.alias());

        modifyRepository(it->alias(), *it).unwrap();
      }
    }

    return expected<void>::success();

    } catch ( ... ) {
      return expected<void>::error( std::current_exception () );
    }

    //! \todo refresh the service automatically if url is changed?
  }


  template <typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::saveService( ServiceInfo & service ) const
  {
    try {

    zypp::filesystem::assert_dir( _options.knownServicesPath );
    zypp::Pathname servfile = generateNonExistingName( _options.knownServicesPath,
      generateFilename( service ) );
    service.setFilepath( servfile );

    MIL << "saving service in " << servfile << std::endl;

    std::ofstream file( servfile.c_str() );
    if ( !file )
    {
      // TranslatorExplanation '%s' is a filename
      ZYPP_THROW( zypp::Exception(zypp::str::form( _("Can't open file '%s' for writing."), servfile.c_str() )));
    }
    service.dumpAsIniOn( file );
    MIL << "done" << std::endl;

    return expected<void>::success();

    } catch ( ... ) {
      return expected<void>::error( std::current_exception () );
    }
  }

  /**
   * Generate a non existing filename in a directory, using a base
   * name. For example if a directory contains 3 files
   *
   * |-- bar
   * |-- foo
   * `-- moo
   *
   * If you try to generate a unique filename for this directory,
   * based on "ruu" you will get "ruu", but if you use the base
   * "foo" you will get "foo_1"
   *
   * \param dir Directory where the file needs to be unique
   * \param basefilename string to base the filename on.
   */
  template <typename ZyppContextType>
  zypp::Pathname RepoManager<ZyppContextType>::generateNonExistingName( const zypp::Pathname & dir,
    const std::string & basefilename ) const
  {
    std::string final_filename = basefilename;
    int counter = 1;
    while ( zypp::PathInfo(dir + final_filename).isExist() )
    {
      final_filename = basefilename + "_" + zypp::str::numstring(counter);
      ++counter;
    }
    return dir + zypp::Pathname(final_filename);
  }

  template <typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::touchIndexFile(const RepoInfo &info, const RepoManagerOptions &options)
  {
    try {
    zypp::Pathname productdatapath = rawproductdata_path_for_repoinfo( options, info ).unwrap();

    zypp::repo::RepoType repokind = info.type();
    if ( repokind.toEnum() == zypp::repo::RepoType::NONE_e )
      // unknown, probe the local metadata
      repokind = probeCache( productdatapath );
    // if still unknown, just return
    if (repokind == zypp::repo::RepoType::NONE_e)
      return expected<void>::success();

    zypp::Pathname p;
    switch ( repokind.toEnum() )
    {
      case zypp::repo::RepoType::RPMMD_e :
        p = zypp::Pathname(productdatapath + "/repodata/repomd.xml");
        break;

      case zypp::repo::RepoType::YAST2_e :
        p = zypp::Pathname(productdatapath + "/content");
        break;

      case zypp::repo::RepoType::RPMPLAINDIR_e :
        p = zypp::Pathname(productdatapath + "/cookie");
        break;

      case zypp::repo::RepoType::NONE_e :
      default:
        break;
    }

    // touch the file, ignore error (they are logged anyway)
    zypp::filesystem::touch(p);
    } catch ( ... )  {
      return expected<void>::error( ZYPP_FWD_CURRENT_EXCPT() );
    }
    return expected<void>::success();
  }

  template <typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::touchIndexFile( const RepoInfo & info )
  {
    return touchIndexFile( info, _options );
  }

  template <typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::init_knownServices()
  {
    try {
    zypp::Pathname dir = _options.knownServicesPath;
    std::list<zypp::Pathname> entries;
    if (zypp::PathInfo(dir).isExist())
    {
      if ( zypp::filesystem::readdir( entries, dir, false ) != 0 )
      {
        // TranslatorExplanation '%s' is a pathname
        ZYPP_THROW(zypp::Exception(zypp::str::form(_("Failed to read directory '%s'"), dir.c_str())));
      }

      //str::regex allowedServiceExt("^\\.service(_[0-9]+)?$");
      for_(it, entries.begin(), entries.end() )
      {
        parser::ServiceFileReader( _zyppContext, *it, ServiceCollector(_services));
      }
    }

    repo::PluginServices( _zyppContext, _options.pluginsPath/"services", ServiceCollector(_services) );

    return expected<void>::success();

    } catch ( ... ) {
      return expected<void>::error( std::current_exception () );
    }

  }

  namespace {
    /** Delete \a cachePath_r subdirs not matching known aliases in \a repoEscAliases_r (must be sorted!)
     * \note bnc#891515: Auto-cleanup only zypp.conf default locations. Otherwise
     * we'd need some magic file to identify zypp cache directories. Without this
     * we may easily remove user data (zypper --pkg-cache-dir . download ...)
     * \note bsc#1210740: Don't cleanup if read-only mode was promised.
     */
    inline void cleanupNonRepoMetadtaFolders( const zypp::Pathname & cachePath_r,
                                              const zypp::Pathname & defaultCachePath_r,
                                              const std::list<std::string> & repoEscAliases_r )
    {
      if ( zypp::zypp_readonly_hack::IGotIt() )
        return;

      if ( cachePath_r != defaultCachePath_r )
        return;

      std::list<std::string> entries;
      if ( zypp::filesystem::readdir( entries, cachePath_r, false ) == 0 )
      {
        entries.sort();
        std::set<std::string> oldfiles;
        set_difference( entries.begin(), entries.end(), repoEscAliases_r.begin(), repoEscAliases_r.end(),
                        std::inserter( oldfiles, oldfiles.end() ) );

        // bsc#1178966: Files or symlinks here have been created by the user
        // for whatever purpose. It's our cache, so we purge them now before
        // they may later conflict with directories we need.
        zypp::PathInfo pi;
        for ( const std::string & old : oldfiles )
        {
          if ( old == zypp::Repository::systemRepoAlias() )	// don't remove the @System solv file
            continue;
          pi( cachePath_r/old );
          if ( pi.isDir() )
            zypp::filesystem::recursive_rmdir( pi.path() );
          else
            zypp::filesystem::unlink( pi.path() );
        }
      }
    }
  } // namespace

  template <typename ZyppContextType>
  expected<void> RepoManager<ZyppContextType>::init_knownRepositories()
  {
    try {

    MIL << "start construct known repos" << std::endl;

    if ( zypp::PathInfo(_options.knownReposPath).isExist() )
    {
      std::list<std::string> repoEscAliases;
      std::list<RepoInfo> orphanedRepos;
      for ( const RepoInfo & rI : repositories_in_dir( _zyppContext, _zyppContext->progressObserver(), _options.knownReposPath ) )
      {
        RepoInfo repoInfo = rI;

        // initialize the paths'
        prepareRepoInfo(repoInfo).unwrap();
        // remember it
        _reposX.insert( std::make_pair( repoInfo.alias(), repoInfo ) );	// direct access via _reposX in ctor! no reposManip.

        // detect orphaned repos belonging to a deleted service
        const std::string & serviceAlias( repoInfo.service() );
        if ( ! ( serviceAlias.empty() || hasService( serviceAlias ) ) )
        {
          WAR << "Schedule orphaned service repo for deletion: " << repoInfo << std::endl;
          orphanedRepos.push_back( repoInfo );
          continue;	// don't remember it in repoEscAliases
        }

        repoEscAliases.push_back(repoInfo.escaped_alias());
      }

      // Cleanup orphanded service repos:
      if ( ! orphanedRepos.empty() )
      {
        for ( const auto & repoInfo : orphanedRepos )
        {
          MIL << "Delete orphaned service repo " << repoInfo.alias() << std::endl;
          // translators: Cleanup a repository previously owned by a meanwhile unknown (deleted) service.
          //   %1% = service name
          //   %2% = repository name
          JobReportHelper(_zyppContext, _zyppContext->progressObserver()).warning( zypp::str::Format(_("Unknown service '%1%': Removing orphaned service repository '%2%'"))
                              % repoInfo.service()
                              % repoInfo.alias() );
          try {
            RepoInfo ri = repoInfo;
            removeRepository( ri ).unwrap();
          }
          catch ( const zypp::Exception & caugth )
          {
            JobReportHelper(_zyppContext, _zyppContext->progressObserver()).error( caugth.asUserHistory() );
          }
        }
      }

      // bsc#1210740: Don't cleanup if read-only mode was promised.
      if ( not zypp::zypp_readonly_hack::IGotIt() ) {
        // delete metadata folders without corresponding repo (e.g. old tmp directories)
        //
        // bnc#891515: Auto-cleanup only zypp.conf default locations. Otherwise
        // we'd need somemagic file to identify zypp cache directories. Without this
        // we may easily remove user data (zypper --pkg-cache-dir . download ...)
        repoEscAliases.sort();
        cleanupNonRepoMetadtaFolders( _options.repoRawCachePath,
                                      zypp::Pathname::assertprefix( _options.rootDir, _zyppContext->config().builtinRepoMetadataPath() ),
                                      repoEscAliases );
        cleanupNonRepoMetadtaFolders( _options.repoSolvCachePath,
                                      zypp::Pathname::assertprefix( _options.rootDir, _zyppContext->config().builtinRepoSolvfilesPath() ),
                                      repoEscAliases );
        // bsc#1204956: Tweak to prevent auto pruning package caches
        if ( autoPruneInDir( _options.repoPackagesCachePath ) )
          cleanupNonRepoMetadtaFolders( _options.repoPackagesCachePath,
                                        zypp::Pathname::assertprefix( _options.rootDir, _zyppContext->config().builtinRepoPackagesPath() ),
                                        repoEscAliases );
      }
    }
    MIL << "end construct known repos" << std::endl;

    return expected<void>::success();

    } catch ( ... ) {
      return expected<void>::error( std::current_exception () );
    }
  }

  // explicitely intantiate the template types we want to work with
  template class RepoManager<SyncContext>;
  template class RepoManager<AsyncContext>;
} // namespace zyppng
