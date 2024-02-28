/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "repomanagerbase_p.h"

#include <solv/solvversion.h>

#include <zypp-core/base/Regex.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp/HistoryLog.h>
#include <zypp/ZConfig.h>
#include <zypp/ZYppCallbacks.h>
#include <zypp/base/LogTools.h>
#include <zypp/parser/RepoFileReader.h>
#include <zypp/parser/ServiceFileReader.h>
#include <zypp/sat/Pool.h>
#include <zypp/zypp_detail/urlcredentialextractor_p.h>
#include <zypp/repo/ServiceType.h>
#include <zypp/repo/PluginServices.h>

#include <fstream>

namespace zypp
{

  namespace zypp_readonly_hack {
    bool IGotIt(); // in readonly-mode
  }

  namespace {
    /** Delete \a cachePath_r subdirs not matching known aliases in \a repoEscAliases_r (must be sorted!)
     * \note bnc#891515: Auto-cleanup only zypp.conf default locations. Otherwise
     * we'd need some magic file to identify zypp cache directories. Without this
     * we may easily remove user data (zypper --pkg-cache-dir . download ...)
     */
    inline void cleanupNonRepoMetadataFolders( const Pathname & cachePath_r,
      const Pathname & defaultCachePath_r,
      const std::list<std::string> & repoEscAliases_r )
    {
      if ( cachePath_r != defaultCachePath_r )
        return;

      std::list<std::string> entries;
      if ( filesystem::readdir( entries, cachePath_r, false ) == 0 )
      {
        entries.sort();
        std::set<std::string> oldfiles;
        set_difference( entries.begin(), entries.end(), repoEscAliases_r.begin(), repoEscAliases_r.end(),
          std::inserter( oldfiles, oldfiles.end() ) );

        // bsc#1178966: Files or symlinks here have been created by the user
        // for whatever purpose. It's our cache, so we purge them now before
        // they may later conflict with directories we need.
        PathInfo pi;
        for ( const std::string & old : oldfiles )
        {
          if ( old == Repository::systemRepoAlias() )	// don't remove the @System solv file
            continue;
          pi( cachePath_r/old );
          if ( pi.isDir() )
            filesystem::recursive_rmdir( pi.path() );
          else
            filesystem::unlink( pi.path() );
        }
      }
    }
  } // namespace

  std::string filenameFromAlias(const std::string &alias_r, const std::string &stem_r)
  {
    std::string filename( alias_r );
    // replace slashes with underscores
    str::replaceAll( filename, "/", "_" );

    filename = Pathname(filename).extend("."+stem_r).asString();
    MIL << "generating filename for " << stem_r << " [" << alias_r << "] : '" << filename << "'" << endl;
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
        << targetDistro << "')." << endl;

      return true;
    }

    repos.push_back(repo);
    return true;
  }

  std::list<RepoInfo> repositories_in_file(const Pathname &file)
  {
    MIL << "repo file: " << file << endl;
    RepoCollector collector;
    parser::RepoFileReader parser( file, bind( &RepoCollector::collect, &collector, _1 ) );
    return std::move(collector.repos);
  }

  std::list<RepoInfo> repositories_in_dir(const Pathname &dir)
  {
    MIL << "directory " << dir << endl;
    std::list<RepoInfo> repos;
    bool nonroot( geteuid() != 0 );
    if ( nonroot && ! PathInfo(dir).userMayRX() )
    {
      JobReport::warning( str::Format(_("Cannot read repo directory '%1%': Permission denied")) % dir );
    }
    else
    {
      std::list<Pathname> entries;
      if ( filesystem::readdir( entries, dir, false ) != 0 )
      {
        // TranslatorExplanation '%s' is a pathname
        ZYPP_THROW(Exception(str::form(_("Failed to read directory '%s'"), dir.c_str())));
      }

      str::regex allowedRepoExt("^\\.repo(_[0-9]+)?$");
      for ( std::list<Pathname>::const_iterator it = entries.begin(); it != entries.end(); ++it )
      {
        if ( str::regex_match(it->extension(), allowedRepoExt) )
        {
          if ( nonroot && ! PathInfo(*it).userMayR() )
          {
            JobReport::warning( str::Format(_("Cannot read repo file '%1%': Permission denied")) % *it );
          }
          else
          {
            const std::list<RepoInfo> & tmp( repositories_in_file( *it ) );
            repos.insert( repos.end(), tmp.begin(), tmp.end() );
          }
        }
      }
    }
    return repos;
  }

  void assert_urls(const RepoInfo &info)
  {
    if ( info.baseUrlsEmpty() )
      ZYPP_THROW( repo::RepoNoUrlException( info ) );
  }

  bool autoPruneInDir(const Pathname &path_r)
  { return not zypp::PathInfo(path_r/".no_auto_prune").isExist(); }


  RepoManagerBaseImpl::RepoManagerBaseImpl(const RepoManagerOptions &opt)
    : _options(opt)
  { }

  RepoManagerBaseImpl::~RepoManagerBaseImpl()
  {
    //@TODO Add Appdata refresh?
  }

  RepoStatus RepoManagerBaseImpl::metadataStatus(const RepoInfo & info , const RepoManagerOptions &options)
  {
    Pathname mediarootpath = rawcache_path_for_repoinfo( options, info );
    Pathname productdatapath = rawproductdata_path_for_repoinfo( options, info );

    repo::RepoType repokind = info.type();
    // If unknown, probe the local metadata
    if ( repokind == repo::RepoType::NONE )
      repokind = probeCache( productdatapath );

    // NOTE: The calling code expects an empty RepoStatus being returned
    // if the metadata cache is empty. So additioanl components like the
    // RepoInfos status are joined after the switch IFF the status is not
    // empty.
    RepoStatus status;
    switch ( repokind.toEnum() )
    {
      case repo::RepoType::RPMMD_e :
        status = RepoStatus( productdatapath/"repodata/repomd.xml");
        if ( info.requireStatusWithMediaFile() )
          status = status && RepoStatus( mediarootpath/"media.1/media" );
        break;

      case repo::RepoType::YAST2_e :
        status = RepoStatus( productdatapath/"content" ) && RepoStatus( mediarootpath/"media.1/media" );
        break;

      case repo::RepoType::RPMPLAINDIR_e :
        status = RepoStatus::fromCookieFile( productdatapath/"cookie" );	// dir status at last refresh
        break;

      case repo::RepoType::NONE_e :
        // Return default RepoStatus in case of RepoType::NONE
        // indicating it should be created?
        // ZYPP_THROW(RepoUnknownTypeException());
        break;
    }

    if ( ! status.empty() )
      status = status && RepoStatus( info );

    return status;
  }

  RepoStatus RepoManagerBaseImpl::metadataStatus(const RepoInfo &info) const
  {
    return metadataStatus( info, _options );
  }

  void RepoManagerBaseImpl::cleanMetadata(const RepoInfo &info, const ProgressData::ReceiverFnc & progressfnc )
  {
    ProgressData progress(100);
    progress.sendTo(progressfnc);
    filesystem::recursive_rmdir( ZConfig::instance().geoipCachePath() );
    filesystem::recursive_rmdir( rawcache_path_for_repoinfo(_options, info) );
    progress.toMax();
  }

  void RepoManagerBaseImpl::cleanPackages(const RepoInfo &info, const ProgressData::ReceiverFnc & progressfnc, bool isAutoClean  )
  {
    ProgressData progress(100);
    progress.sendTo(progressfnc);

    // bsc#1204956: Tweak to prevent auto pruning package caches
    const Pathname & rpc { packagescache_path_for_repoinfo(_options, info) };
    if ( not isAutoClean || autoPruneInDir( rpc.dirname() ) )
      filesystem::recursive_rmdir( rpc );

    progress.toMax();
  }

  /** Probe Metadata in a local cache directory
   *
   * \note Metadata in local cache directories must not be probed using \ref probe as
   * a cache path must not be rewritten (bnc#946129)
   */
  repo::RepoType RepoManagerBaseImpl::probeCache( const Pathname & path_r )
  {
    MIL << "going to probe the cached repo at " << path_r << endl;

    repo::RepoType ret = repo::RepoType::NONE;

    if ( PathInfo(path_r/"/repodata/repomd.xml").isFile() )
    { ret = repo::RepoType::RPMMD; }
    else if ( PathInfo(path_r/"/content").isFile() )
    { ret = repo::RepoType::YAST2; }
    else if ( PathInfo(path_r).isDir() )
    { ret = repo::RepoType::RPMPLAINDIR; }

    MIL << "Probed cached type " << ret << " at " << path_r << endl;
    return ret;
  }

  void RepoManagerBaseImpl::cleanCacheDirGarbage( const ProgressData::ReceiverFnc & progressrcv )
  {
    MIL << "Going to clean up garbage in cache dirs" << endl;

    ProgressData progress(300);
    progress.sendTo(progressrcv);
    progress.toMin();

    std::list<Pathname> cachedirs;
    cachedirs.push_back(_options.repoRawCachePath);
    cachedirs.push_back(_options.repoPackagesCachePath);
    cachedirs.push_back(_options.repoSolvCachePath);

    for_( dir, cachedirs.begin(), cachedirs.end() )
    {
      if ( PathInfo(*dir).isExist() )
      {
        std::list<Pathname> entries;
        if ( filesystem::readdir( entries, *dir, false ) != 0 )
          // TranslatorExplanation '%s' is a pathname
          ZYPP_THROW(Exception(str::form(_("Failed to read directory '%s'"), dir->c_str())));

        unsigned sdircount   = entries.size();
        unsigned sdircurrent = 1;
        for_( subdir, entries.begin(), entries.end() )
        {
          // if it does not belong known repo, make it disappear
          bool found = false;
          for_( r, repoBegin(), repoEnd() )
            if ( subdir->basename() == r->escaped_alias() )
          { found = true; break; }

          if ( ! found && ( Date::now()-PathInfo(*subdir).mtime() > Date::day ) )
            filesystem::recursive_rmdir( *subdir );

          progress.set( progress.val() + sdircurrent * 100 / sdircount );
          ++sdircurrent;
        }
      }
      else
        progress.set( progress.val() + 100 );
    }
    progress.toMax();
  }

  void RepoManagerBaseImpl::cleanCache(const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  {
    ProgressData progress(100);
    progress.sendTo(progressrcv);
    progress.toMin();

    MIL << "Removing raw metadata cache for " << info.alias() << endl;
    filesystem::recursive_rmdir(solv_path_for_repoinfo(_options, info));

    progress.toMax();
  }

  void RepoManagerBaseImpl::loadFromCache( const RepoInfo & info, const ProgressData::ReceiverFnc & progressrcv )
  {
    assert_alias(info);
    Pathname solvfile = solv_path_for_repoinfo(_options, info) / "solv";

    if ( ! PathInfo(solvfile).isExist() )
      ZYPP_THROW(repo::RepoNotCachedException(info));

    sat::Pool::instance().reposErase( info.alias() );

    Repository repo = sat::Pool::instance().addRepoSolv( solvfile, info );
    // test toolversion in order to rebuild solv file in case
    // it was written by a different libsolv-tool parser.
    const std::string & toolversion( sat::LookupRepoAttr( sat::SolvAttr::repositoryToolVersion, repo ).begin().asString() );
    if ( toolversion != LIBSOLV_TOOLVERSION ) {
      repo.eraseFromPool();
      ZYPP_THROW(Exception(str::Str() << "Solv-file was created by '"<<toolversion<<"'-parser (want "<<LIBSOLV_TOOLVERSION<<")."));
    }
  }

  void RepoManagerBaseImpl::addProbedRepository(const RepoInfo &info, repo::RepoType probedType)
  {

    auto tosave = info;

    // assert the directory exists
    filesystem::assert_dir(_options.knownReposPath);

    Pathname repofile = generateNonExistingName(
      _options.knownReposPath, generateFilename(tosave));
    // now we have a filename that does not exists
    MIL << "Saving repo in " << repofile << endl;

    std::ofstream file(repofile.c_str());
    if (!file)
    {
      // TranslatorExplanation '%s' is a filename
      ZYPP_THROW( Exception(str::form( _("Can't open file '%s' for writing."), repofile.c_str() )));
    }

    tosave.dumpAsIniOn(file);
    tosave.setFilepath(repofile);
    tosave.setMetadataPath( rawcache_path_for_repoinfo( _options, tosave ) );
    tosave.setPackagesPath( packagescache_path_for_repoinfo( _options, tosave ) );
    {
      // We should fix the API as we must inject those paths
      // into the repoinfo in order to keep it usable.
      RepoInfo & oinfo( const_cast<RepoInfo &>(info) );
      oinfo.setFilepath(repofile);
      oinfo.setMetadataPath( rawcache_path_for_repoinfo( _options, tosave ) );
      oinfo.setPackagesPath( packagescache_path_for_repoinfo( _options, tosave ) );
    }
    reposManip().insert(tosave);

    // check for credentials in Urls
    UrlCredentialExtractor( _options.rootDir ).collect( tosave.baseUrls() );

    HistoryLog(_options.rootDir).addRepository(tosave);
  }

  void RepoManagerBaseImpl::removeRepositoryImpl( const RepoInfo & info, const ProgressData::ReceiverFnc & progressrcv )
  {
    ProgressData progress;
    callback::SendReport<ProgressReport> report;
    progress.sendTo( ProgressReportAdaptor( progressrcv, report ) );
    progress.name(str::form(_("Removing repository '%s'"), info.label().c_str()));

    MIL << "Going to delete repo " << info.alias() << endl;

    for_( it, repoBegin(), repoEnd() )
    {
      // they can be the same only if the provided is empty, that means
      // the provided repo has no alias
      // then skip
      if ( (!info.alias().empty()) && ( info.alias() != (*it).alias() ) )
        continue;

      // TODO match by url

      // we have a matcing repository, now we need to know
      // where it does come from.
      RepoInfo todelete = *it;
      if (todelete.filepath().empty())
      {
        ZYPP_THROW(repo::RepoException( todelete, _("Can't figure out where the repo is stored.") ));
      }
      else
      {
        // figure how many repos are there in the file:
        std::list<RepoInfo> filerepos = repositories_in_file(todelete.filepath());
        if ( filerepos.size() == 0	// bsc#984494: file may have already been deleted
             ||(filerepos.size() == 1 && filerepos.front().alias() == todelete.alias() ) )
        {
          // easy: file does not exist, contains no or only the repo to delete: delete the file
          int ret = filesystem::unlink( todelete.filepath() );
          if ( ! ( ret == 0 || ret == ENOENT ) )
          {
            // TranslatorExplanation '%s' is a filename
            ZYPP_THROW(repo::RepoException( todelete, str::form( _("Can't delete '%s'"), todelete.filepath().c_str() )));
          }
          MIL << todelete.alias() << " successfully deleted." << endl;
        }
        else
        {
          // there are more repos in the same file
          // write them back except the deleted one.
          //TmpFile tmp;
          //std::ofstream file(tmp.path().c_str());

          // assert the directory exists
          filesystem::assert_dir(todelete.filepath().dirname());

          std::ofstream file(todelete.filepath().c_str());
          if (!file)
          {
            // TranslatorExplanation '%s' is a filename
            ZYPP_THROW( Exception(str::form( _("Can't open file '%s' for writing."), todelete.filepath().c_str() )));
          }
          for ( std::list<RepoInfo>::const_iterator fit = filerepos.begin();
                fit != filerepos.end();
                ++fit )
          {
            if ( (*fit).alias() != todelete.alias() )
              (*fit).dumpAsIniOn(file);
          }
        }

        CombinedProgressData cSubprogrcv(progress, 20);
        CombinedProgressData mSubprogrcv(progress, 40);
        CombinedProgressData pSubprogrcv(progress, 40);
        // now delete it from cache
        if ( isCached(todelete) )
          cleanCache( todelete, cSubprogrcv);
        // now delete metadata (#301037)
        cleanMetadata( todelete, mSubprogrcv );
        cleanPackages( todelete, pSubprogrcv, true/*isAutoClean*/ );
        reposManip().erase(todelete);
        MIL << todelete.alias() << " successfully deleted." << endl;
        HistoryLog(_options.rootDir).removeRepository(todelete);
        return;
      } // else filepath is empty

    }
    // should not be reached on a sucess workflow
    ZYPP_THROW(repo::RepoNotFoundException(info));
  }

  void RepoManagerBaseImpl::modifyRepository( const std::string & alias, const RepoInfo & newinfo_r, const ProgressData::ReceiverFnc & progressrcv )
  {
    RepoInfo toedit = getRepositoryInfo(alias);
    RepoInfo newinfo( newinfo_r ); // need writable copy to upadte housekeeping data

    // check if the new alias already exists when renaming the repo
    if ( alias != newinfo.alias() && hasRepo( newinfo.alias() ) )
    {
      ZYPP_THROW(repo::RepoAlreadyExistsException(newinfo));
    }

    if (toedit.filepath().empty())
    {
      ZYPP_THROW(repo::RepoException( toedit, _("Can't figure out where the repo is stored.") ));
    }
    else
    {
      // figure how many repos are there in the file:
      std::list<RepoInfo> filerepos = repositories_in_file(toedit.filepath());

      // there are more repos in the same file
      // write them back except the deleted one.
      //TmpFile tmp;
      //std::ofstream file(tmp.path().c_str());

      // assert the directory exists
      filesystem::assert_dir(toedit.filepath().dirname());

      std::ofstream file(toedit.filepath().c_str());
      if (!file)
      {
        // TranslatorExplanation '%s' is a filename
        ZYPP_THROW( Exception(str::form( _("Can't open file '%s' for writing."), toedit.filepath().c_str() )));
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

      if ( toedit.enabled() && !newinfo.enabled() )
      {
        // On the fly remove solv.idx files for bash completion if a repo gets disabled.
        const Pathname & solvidx = solv_path_for_repoinfo(_options, newinfo)/"solv.idx";
        if ( PathInfo(solvidx).isExist() )
          filesystem::unlink( solvidx );
      }

      newinfo.setFilepath(toedit.filepath());
      newinfo.setMetadataPath( rawcache_path_for_repoinfo( _options, newinfo ) );
      newinfo.setPackagesPath( packagescache_path_for_repoinfo( _options, newinfo ) );
      {
        // We should fix the API as we must inject those paths
        // into the repoinfo in order to keep it usable.
        RepoInfo & oinfo( const_cast<RepoInfo &>(newinfo_r) );
        oinfo.setFilepath(toedit.filepath());
        oinfo.setMetadataPath( rawcache_path_for_repoinfo( _options, newinfo ) );
        oinfo.setPackagesPath( packagescache_path_for_repoinfo( _options, newinfo ) );
      }
      reposManip().erase(toedit);
      reposManip().insert(newinfo);
      // check for credentials in Urls
      UrlCredentialExtractor( _options.rootDir ).collect( newinfo.baseUrls() );
      HistoryLog(_options.rootDir).modifyRepository(toedit, newinfo);
      MIL << "repo " << alias << " modified" << endl;
    }
  }

  RepoInfo RepoManagerBaseImpl::getRepositoryInfo( const std::string & alias  )
  {
    RepoConstIterator it( findAlias( alias, repos() ) );
    if ( it != repos().end() )
      return *it;
    RepoInfo info;
    info.setAlias( alias );
    ZYPP_THROW( repo::RepoNotFoundException(info) );
  }


  RepoInfo RepoManagerBaseImpl::getRepositoryInfo( const Url & url, const url::ViewOption & urlview )
  {
    for_( it, repoBegin(), repoEnd() )
    {
      for_( urlit, (*it).baseUrlsBegin(), (*it).baseUrlsEnd() )
      {
        if ( (*urlit).asString(urlview) == url.asString(urlview) )
          return *it;
      }
    }
    RepoInfo info;
    info.setBaseUrl( url );
    ZYPP_THROW( repo::RepoNotFoundException(info) );
  }

  void RepoManagerBaseImpl::addService( const ServiceInfo & service )
  {
    assert_alias( service );

    // check if service already exists
    if ( hasService( service.alias() ) )
      ZYPP_THROW( repo::ServiceAlreadyExistsException( service ) );

    // Writable ServiceInfo is needed to save the location
    // of the .service file. Finaly insert into the service list.
    ServiceInfo toSave( service );
    saveService( toSave );
    _services.insert( toSave );

    // check for credentials in Url
    UrlCredentialExtractor( _options.rootDir ).collect( toSave.url() );

    MIL << "added service " << toSave.alias() << endl;
  }

  ////////////////////////////////////////////////////////////////////////////

  void RepoManagerBaseImpl::removeService( const std::string & alias )
  {
    MIL << "Going to delete service " << alias << endl;

    const ServiceInfo & service = getService( alias );

    Pathname location = service.filepath();
    if( location.empty() )
    {
      ZYPP_THROW(repo::ServiceException( service, _("Can't figure out where the service is stored.") ));
    }

    ServiceSet tmpSet;
    parser::ServiceFileReader( location, ServiceCollector(tmpSet) );

    // only one service definition in the file
    if ( tmpSet.size() == 1 )
    {
      if ( filesystem::unlink(location) != 0 )
      {
        // TranslatorExplanation '%s' is a filename
        ZYPP_THROW(repo::ServiceException( service, str::form( _("Can't delete '%s'"), location.c_str() ) ));
      }
      MIL << alias << " successfully deleted." << endl;
    }
    else
    {
      filesystem::assert_dir(location.dirname());

      std::ofstream file(location.c_str());
      if( !file )
      {
        // TranslatorExplanation '%s' is a filename
        ZYPP_THROW( Exception(str::form( _("Can't open file '%s' for writing."), location.c_str() )));
      }

      for_(it, tmpSet.begin(), tmpSet.end())
      {
        if( it->alias() != alias )
          it->dumpAsIniOn(file);
      }

      MIL << alias << " successfully deleted from file " << location <<  endl;
    }

    // now remove all repositories added by this service
    RepoCollector rcollector;
    getRepositoriesInService( alias,
      boost::make_function_output_iterator( bind( &RepoCollector::collect, &rcollector, _1 ) ) );
    // cannot do this directly in getRepositoriesInService - would invalidate iterators
    for_(rit, rcollector.repos.begin(), rcollector.repos.end())
      removeRepository(*rit);
  }

  void RepoManagerBaseImpl::modifyService( const std::string & oldAlias, const ServiceInfo & newService )
  {
    MIL << "Going to modify service " << oldAlias << endl;

    // we need a writable copy to link it to the file where
    // it is saved if we modify it
    ServiceInfo service(newService);

    if ( service.type() == repo::ServiceType::PLUGIN )
    {
      ZYPP_THROW(repo::ServicePluginImmutableException( service ));
    }

    const ServiceInfo & oldService = getService(oldAlias);

    Pathname location = oldService.filepath();
    if( location.empty() )
    {
      ZYPP_THROW(repo::ServiceException( oldService, _("Can't figure out where the service is stored.") ));
    }

    // remember: there may multiple services being defined in one file:
    ServiceSet tmpSet;
    parser::ServiceFileReader( location, ServiceCollector(tmpSet) );

    filesystem::assert_dir(location.dirname());
    std::ofstream file(location.c_str());
    for_(it, tmpSet.begin(), tmpSet.end())
    {
      if( *it != oldAlias )
        it->dumpAsIniOn(file);
    }
    service.dumpAsIniOn(file);
    file.close();
    service.setFilepath(location);

    _services.erase(oldAlias);
    _services.insert(service);
    // check for credentials in Urls
    UrlCredentialExtractor( _options.rootDir ).collect( service.url() );


    // changed properties affecting also repositories
    if ( oldAlias != service.alias()			// changed alias
         || oldService.enabled() != service.enabled() )	// changed enabled status
    {
      std::vector<RepoInfo> toModify;
      getRepositoriesInService(oldAlias, std::back_inserter(toModify));
      for_( it, toModify.begin(), toModify.end() )
      {
        if ( oldService.enabled() != service.enabled() )
        {
          if ( service.enabled() )
          {
            // reset to last refreshs state
            const auto & last = service.repoStates().find( it->alias() );
            if ( last != service.repoStates().end() )
              it->setEnabled( last->second.enabled );
          }
          else
            it->setEnabled( false );
        }

        if ( oldAlias != service.alias() )
          it->setService(service.alias());

        modifyRepository(it->alias(), *it);
      }
    }

    //! \todo refresh the service automatically if url is changed?
  }


  void RepoManagerBaseImpl::saveService( ServiceInfo & service ) const
  {
    filesystem::assert_dir( _options.knownServicesPath );
    Pathname servfile = generateNonExistingName( _options.knownServicesPath,
      generateFilename( service ) );
    service.setFilepath( servfile );

    MIL << "saving service in " << servfile << endl;

    std::ofstream file( servfile.c_str() );
    if ( !file )
    {
      // TranslatorExplanation '%s' is a filename
      ZYPP_THROW( Exception(str::form( _("Can't open file '%s' for writing."), servfile.c_str() )));
    }
    service.dumpAsIniOn( file );
    MIL << "done" << endl;
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
  Pathname RepoManagerBaseImpl::generateNonExistingName( const Pathname & dir,
    const std::string & basefilename ) const
  {
    std::string final_filename = basefilename;
    int counter = 1;
    while ( PathInfo(dir + final_filename).isExist() )
    {
      final_filename = basefilename + "_" + str::numstring(counter);
      ++counter;
    }
    return dir + Pathname(final_filename);
  }

  void RepoManagerBaseImpl::touchIndexFile(const RepoInfo &info, const RepoManagerOptions &options)
  {
    Pathname productdatapath = rawproductdata_path_for_repoinfo( options, info );

    repo::RepoType repokind = info.type();
    if ( repokind.toEnum() == repo::RepoType::NONE_e )
      // unknown, probe the local metadata
      repokind = probeCache( productdatapath );
    // if still unknown, just return
    if (repokind == repo::RepoType::NONE_e)
      return;

    Pathname p;
    switch ( repokind.toEnum() )
    {
      case repo::RepoType::RPMMD_e :
        p = Pathname(productdatapath + "/repodata/repomd.xml");
        break;

      case repo::RepoType::YAST2_e :
        p = Pathname(productdatapath + "/content");
        break;

      case repo::RepoType::RPMPLAINDIR_e :
        p = Pathname(productdatapath + "/cookie");
        break;

      case repo::RepoType::NONE_e :
      default:
        break;
    }

    // touch the file, ignore error (they are logged anyway)
    filesystem::touch(p);
  }

  void RepoManagerBaseImpl::touchIndexFile( const RepoInfo & info )
  {
    return touchIndexFile( info, _options );
  }

  void RepoManagerBaseImpl::init_knownServices()
  {
    Pathname dir = _options.knownServicesPath;
    std::list<Pathname> entries;
    if (PathInfo(dir).isExist())
    {
      if ( filesystem::readdir( entries, dir, false ) != 0 )
      {
        // TranslatorExplanation '%s' is a pathname
        ZYPP_THROW(Exception(str::form(_("Failed to read directory '%s'"), dir.c_str())));
      }

      //str::regex allowedServiceExt("^\\.service(_[0-9]+)?$");
      for_(it, entries.begin(), entries.end() )
      {
        parser::ServiceFileReader(*it, ServiceCollector(_services));
      }
    }

    repo::PluginServices(_options.pluginsPath/"services", ServiceCollector(_services));
  }

  namespace {
    /** Delete \a cachePath_r subdirs not matching known aliases in \a repoEscAliases_r (must be sorted!)
     * \note bnc#891515: Auto-cleanup only zypp.conf default locations. Otherwise
     * we'd need some magic file to identify zypp cache directories. Without this
     * we may easily remove user data (zypper --pkg-cache-dir . download ...)
     * \note bsc#1210740: Don't cleanup if read-only mode was promised.
     */
    inline void cleanupNonRepoMetadtaFolders( const Pathname & cachePath_r,
                                              const Pathname & defaultCachePath_r,
                                              const std::list<std::string> & repoEscAliases_r )
    {
      if ( zypp_readonly_hack::IGotIt() )
        return;

      if ( cachePath_r != defaultCachePath_r )
        return;

      std::list<std::string> entries;
      if ( filesystem::readdir( entries, cachePath_r, false ) == 0 )
      {
        entries.sort();
        std::set<std::string> oldfiles;
        set_difference( entries.begin(), entries.end(), repoEscAliases_r.begin(), repoEscAliases_r.end(),
                        std::inserter( oldfiles, oldfiles.end() ) );

        // bsc#1178966: Files or symlinks here have been created by the user
        // for whatever purpose. It's our cache, so we purge them now before
        // they may later conflict with directories we need.
        PathInfo pi;
        for ( const std::string & old : oldfiles )
        {
          if ( old == Repository::systemRepoAlias() )	// don't remove the @System solv file
            continue;
          pi( cachePath_r/old );
          if ( pi.isDir() )
            filesystem::recursive_rmdir( pi.path() );
          else
            filesystem::unlink( pi.path() );
        }
      }
    }
  } // namespace

  void RepoManagerBaseImpl::init_knownRepositories()
  {
    MIL << "start construct known repos" << endl;

    if ( PathInfo(_options.knownReposPath).isExist() )
    {
      std::list<std::string> repoEscAliases;
      std::list<RepoInfo> orphanedRepos;
      for ( RepoInfo & repoInfo : repositories_in_dir(_options.knownReposPath) )
      {
        // set the metadata path for the repo
        repoInfo.setMetadataPath( rawcache_path_for_repoinfo(_options, repoInfo) );
        // set the downloaded packages path for the repo
        repoInfo.setPackagesPath( packagescache_path_for_repoinfo(_options, repoInfo) );
        // remember it
        _reposX.insert( repoInfo );	// direct access via _reposX in ctor! no reposManip.

        // detect orphaned repos belonging to a deleted service
        const std::string & serviceAlias( repoInfo.service() );
        if ( ! ( serviceAlias.empty() || hasService( serviceAlias ) ) )
        {
          WAR << "Schedule orphaned service repo for deletion: " << repoInfo << endl;
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
          MIL << "Delete orphaned service repo " << repoInfo.alias() << endl;
          // translators: Cleanup a repository previously owned by a meanwhile unknown (deleted) service.
          //   %1% = service name
          //   %2% = repository name
          JobReport::warning( str::Format(_("Unknown service '%1%': Removing orphaned service repository '%2%'"))
                              % repoInfo.service()
                              % repoInfo.alias() );
          try {
            removeRepository( repoInfo );
          }
          catch ( const Exception & caugth )
          {
            JobReport::error( caugth.asUserHistory() );
          }
        }
      }

      // bsc#1210740: Don't cleanup if read-only mode was promised.
      if ( not zypp_readonly_hack::IGotIt() ) {
        // delete metadata folders without corresponding repo (e.g. old tmp directories)
        //
        // bnc#891515: Auto-cleanup only zypp.conf default locations. Otherwise
        // we'd need somemagic file to identify zypp cache directories. Without this
        // we may easily remove user data (zypper --pkg-cache-dir . download ...)
        repoEscAliases.sort();
        cleanupNonRepoMetadtaFolders( _options.repoRawCachePath,
                                      Pathname::assertprefix( _options.rootDir, ZConfig::instance().builtinRepoMetadataPath() ),
                                      repoEscAliases );
        cleanupNonRepoMetadtaFolders( _options.repoSolvCachePath,
                                      Pathname::assertprefix( _options.rootDir, ZConfig::instance().builtinRepoSolvfilesPath() ),
                                      repoEscAliases );
        // bsc#1204956: Tweak to prevent auto pruning package caches
        if ( autoPruneInDir( _options.repoPackagesCachePath ) )
          cleanupNonRepoMetadtaFolders( _options.repoPackagesCachePath,
                                        Pathname::assertprefix( _options.rootDir, ZConfig::instance().builtinRepoPackagesPath() ),
                                        repoEscAliases );
      }
    }
    MIL << "end construct known repos" << endl;
  }

  void assert_alias(const RepoInfo &info)
  {
    if ( info.alias().empty() )
      ZYPP_THROW( repo::RepoNoAliasException( info ) );
    // bnc #473834. Maybe we can match the alias against a regex to define
    // and check for valid aliases
    if ( info.alias()[0] == '.')
      ZYPP_THROW(repo::RepoInvalidAliasException(
        info, _("Repository alias cannot start with dot.")));
  }

}

