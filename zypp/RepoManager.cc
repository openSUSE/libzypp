/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/RepoManager.cc
 *
*/

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <list>
#include <map>
#include <algorithm>
#include <chrono>

#include <zypp-core/base/InputStream>
#include <zypp-core/Digest.h>
#include <zypp/base/LogTools.h>
#include <zypp/base/Gettext.h>
#include <zypp-core/base/DefaultIntegral>
#include <zypp/base/Function.h>
#include <zypp/base/Regex.h>
#include <zypp/PathInfo.h>
#include <zypp/TmpPath.h>

#include <zypp/ServiceInfo.h>
#include <zypp/repo/RepoException.h>
#include <zypp/RepoManager.h>
#include <zypp/zypp_detail/repomanagerbase_p.h>
#include <zypp/zypp_detail/urlcredentialextractor_p.h>

#include <zypp/media/MediaManager.h>
#include <zypp-media/auth/CredentialManager>
#include <zypp-media/MediaException>
#include <zypp/MediaSetAccess.h>
#include <zypp/ExternalProgram.h>
#include <zypp/ManagedFile.h>

#include <zypp/parser/xml/Reader.h>
#include <zypp/repo/ServiceRepos.h>
#include <zypp/repo/PluginServices.h>
#include <zypp/repo/PluginRepoverification.h>

#include <zypp/Target.h> // for Target::targetDistribution() for repo index services
#include <zypp/ZYppFactory.h> // to get the Target from ZYpp instance
#include <zypp/HistoryLog.h> // to write history :O)

#include <zypp/ZYppCallbacks.h>

#include "sat/Pool.h"
#include "zypp-media/ng/providespec.h"
#include <zypp/base/Algorithm.h>


// zyppng related includes
#include <zypp-core/zyppng/pipelines/Lift>
#include <zypp/ng/workflows/contextfacade.h>
#include <zypp/ng/repo/refresh.h>
#include <zypp/ng/repo/workflows/repomanagerwf.h>


using std::endl;
using std::string;
using namespace zypp::repo;

#define OPT_PROGRESS const ProgressData::ReceiverFnc & = ProgressData::ReceiverFnc()

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace env
  {
    /** To trigger appdata refresh unconditionally */
    inline bool ZYPP_PLUGIN_APPDATA_FORCE_COLLECT()
    {
      const char * env = getenv("ZYPP_PLUGIN_APPDATA_FORCE_COLLECT");
      return( env && str::strToBool( env, true ) );
    }
  } // namespace env
  ///////////////////////////////////////////////////////////////////

  namespace
  {
    /** Simple media mounter to access non-downloading URLs e.g. for non-local plaindir repos.
     * \ingroup g_RAII
     */
    class MediaMounter
    {
      public:
        /** Ctor provides media access. */
        MediaMounter( const Url & url_r )
        {
          media::MediaManager mediamanager;
          _mid = mediamanager.open( url_r );
          mediamanager.attach( _mid );
        }

        /** Ctor releases the media. */
        ~MediaMounter()
        {
          media::MediaManager mediamanager;
          mediamanager.release( _mid );
          mediamanager.close( _mid );
        }

        /** Convert a path relative to the media into an absolute path.
         *
         * Called without argument it returns the path to the medias root directory.
        */
        Pathname getPathName( const Pathname & path_r = Pathname() ) const
        {
          media::MediaManager mediamanager;
          return mediamanager.localPath( _mid, path_r );
        }

      private:
        media::MediaAccessId _mid;
    };
    ///////////////////////////////////////////////////////////////////
  } // namespace
  ///////////////////////////////////////////////////////////////////

  std::list<RepoInfo> readRepoFile( const Url & repo_file )
  {
    repo::RepoVariablesUrlReplacer replaceVars;
    Url repoFileUrl { replaceVars(repo_file) };

    ManagedFile local = MediaSetAccess::provideFileFromUrl( repoFileUrl );
    DBG << "reading repo file " << repoFileUrl << ", local path: " << local << endl;

    return repositories_in_file(local);
  }

  ///////////////////////////////////////////////////////////////////
  /// \class RepoManager::Impl
  /// \brief RepoManager implementation.
  ///
  ///////////////////////////////////////////////////////////////////
  struct ZYPP_LOCAL RepoManager::Impl : public RepoManagerBaseImpl
  {
  public:
    Impl( RepoManagerOptions &&opt )
      : RepoManagerBaseImpl( std::move(opt) )
      , _pluginRepoverification( _options.pluginsPath/"repoverification", _options.rootDir )
    {
      init_knownServices();
      init_knownRepositories();
    }

    ~Impl()
    {
      // trigger appdata refresh if some repos change
      if ( ( _reposDirty || env::ZYPP_PLUGIN_APPDATA_FORCE_COLLECT() )
        && geteuid() == 0 && ( _options.rootDir.empty() || _options.rootDir == "/" ) )
      {
        try {
          std::list<Pathname> entries;
          filesystem::readdir( entries, _options.pluginsPath/"appdata", false );
          if ( ! entries.empty() )
          {
            ExternalProgram::Arguments cmd;
            cmd.push_back( "<" );		// discard stdin
            cmd.push_back( ">" );		// discard stdout
            cmd.push_back( "PROGRAM" );		// [2] - fix index below if changing!
            for ( const auto & rinfo : repos() )
            {
              if ( ! rinfo.enabled() )
                continue;
              cmd.push_back( "-R" );
              cmd.push_back( rinfo.alias() );
              cmd.push_back( "-t" );
              cmd.push_back( rinfo.type().asString() );
              cmd.push_back( "-p" );
              cmd.push_back( (rinfo.metadataPath()/rinfo.path()).asString() ); // bsc#1197684: path to the repodata/ directory inside the cache
            }

            for_( it, entries.begin(), entries.end() )
            {
              PathInfo pi( *it );
              //DBG << "/tmp/xx ->" << pi << endl;
              if ( pi.isFile() && pi.userMayRX() )
              {
                // trigger plugin
                cmd[2] = pi.asString();		// [2] - PROGRAM
                ExternalProgram prog( cmd, ExternalProgram::Stderr_To_Stdout );
              }
            }
          }
        }
        catch (...) {}	// no throw in dtor
      }
    }

  public:
    RefreshCheckStatus checkIfToRefreshMetadata( const RepoInfo & info, const Url & url, RawMetadataRefreshPolicy policy );

    void refreshMetadata( const RepoInfo & info, RawMetadataRefreshPolicy policy, OPT_PROGRESS );

    void buildCache( const RepoInfo & info, CacheBuildPolicy policy, OPT_PROGRESS );

    repo::RepoType probe( const Url & url, const Pathname & path = Pathname() ) const;

    void loadFromCache( const RepoInfo & info, OPT_PROGRESS );

    void addRepository( const RepoInfo & info, OPT_PROGRESS );

    void addRepositories( const Url & url, OPT_PROGRESS );

    void removeRepository ( const RepoInfo & info, OPT_PROGRESS ) override;

  public:
    void refreshServices( const RefreshServiceOptions & options_r );

    void refreshService( const std::string & alias, const RefreshServiceOptions & options_r );
    void refreshService( const ServiceInfo & service, const RefreshServiceOptions & options_r )
    {  refreshService( service.alias(), options_r ); }

    repo::ServiceType probeService( const Url & url ) const;

    void refreshGeoIPData ( const RepoInfo::url_set &urls );

  private:
    zypp_private::repo::PluginRepoverification _pluginRepoverification;

  private:
    friend Impl * rwcowClone<Impl>( const Impl * rhs );
    /** clone for RWCOW_pointer */
    Impl * clone() const
    { return new Impl( *this ); }
  };
  ///////////////////////////////////////////////////////////////////

  /** \relates RepoManager::Impl Stream output */
  inline std::ostream & operator<<( std::ostream & str, const RepoManager::Impl & obj )
  { return str << "RepoManager::Impl"; }

  RepoManager::RefreshCheckStatus RepoManager::Impl::checkIfToRefreshMetadata( const RepoInfo & info, const Url & url, RawMetadataRefreshPolicy policy )
  {
    using namespace zyppng::operators;
    using zyppng::operators::operator|;

    refreshGeoIPData( { url } );

    auto ctx = zyppng::SyncContext::create();
    auto res = zyppng::repo::SyncRefreshContext::create( ctx, info, _options )
    | and_then( [&]( zyppng::repo::SyncRefreshContextRef &&refCtx ) {
      refCtx->setPolicy ( static_cast<zyppng::repo::RawMetadataRefreshPolicy>( policy ) );
      return ctx->provider()->attachMedia( url, zyppng::ProvideMediaSpec() )
          | and_then( [ r = std::move(refCtx) ]( auto &&mediaHandle ) mutable { return zyppng::RepoManagerWorkflow::checkIfToRefreshMetadata ( std::move(r), mediaHandle, nullptr ); } );
    } );

    if ( !res ) {
      ZYPP_RETHROW ( res.error() );
    }
    return static_cast<RepoManager::RefreshCheckStatus>(res.get());
  }


  void RepoManager::Impl::refreshMetadata( const RepoInfo & info, RawMetadataRefreshPolicy policy, const ProgressData::ReceiverFnc & progress )
  {
    using namespace zyppng;
    using namespace zyppng::operators;
    using zyppng::operators::operator|;

    // make sure geoIP data is up 2 date
    refreshGeoIPData( info.baseUrls() );

    // Suppress (interactive) media::MediaChangeReport if we in have multiple basurls (>1)
    media::ScopedDisableMediaChangeReport guard( info.baseUrlsSize() > 1 );

    // we will throw this later if no URL checks out fine
    RepoException rexception( info, PL_("Valid metadata not found at specified URL",
                                        "Valid metadata not found at specified URLs",
                                        info.baseUrlsSize() ) );


    auto ctx = SyncContext::create();

    // helper callback in case the repo type changes on the remote
    const auto &updateProbedType = [&]( repo::RepoType repokind ) {
      // update probed type only for repos in system
      for_( it, repoBegin(), repoEnd() )
      {
        if ( info.alias() == (*it).alias() )
        {
          RepoInfo modifiedrepo = *it;
          modifiedrepo.setType( repokind );
          // don't modify .repo in refresh.
          // modifyRepository( info.alias(), modifiedrepo );
          break;
        }
      }
    };

    // try urls one by one
    for ( RepoInfo::urls_const_iterator it = info.baseUrlsBegin(); it != info.baseUrlsEnd(); ++it )
    {
      try {
        auto res = zyppng::repo::SyncRefreshContext::create( ctx, info, _options )
            | and_then( [&]( zyppng::repo::SyncRefreshContextRef refCtx ) {
              refCtx->setPolicy( static_cast<zyppng::repo::RawMetadataRefreshPolicy>( policy ) );
              // in case probe detects a different repokind, update our internal repos
              refCtx->connectFunc( &zyppng::repo::SyncRefreshContext::sigProbedTypeChanged, updateProbedType );
              return ctx->provider()->attachMedia( *it, zyppng::ProvideMediaSpec() )
                     | and_then( [ refCtx ]( auto mediaHandle ) mutable { return zyppng::RepoManagerWorkflow::refreshMetadata ( std::move(refCtx), std::move(mediaHandle), nullptr); } );
        });

        if ( !res ) {
          ZYPP_RETHROW( res.error() );
        }
        if ( ! isTmpRepo( info ) )
          reposManip();	// remember to trigger appdata refresh

        // we are done.
        return;

      } catch ( const zypp::Exception &e ) {
        ERR << "Trying another url..." << endl;

        // remember the exception caught for the *first URL*
        // if all other URLs fail, the rexception will be thrown with the
        // cause of the problem of the first URL remembered
        if (it == info.baseUrlsBegin())
          rexception.remember( e );
        else
          rexception.addHistory( e.asUserString() );
      }
    } // for every url
    ERR << "No more urls..." << endl;
    ZYPP_THROW(rexception);

  }

  void RepoManager::Impl::buildCache( const RepoInfo & info, CacheBuildPolicy policy, const ProgressData::ReceiverFnc & progressrcv )
  {
    assert_alias(info);
    Pathname mediarootpath = rawcache_path_for_repoinfo( _options, info );
    Pathname productdatapath = rawproductdata_path_for_repoinfo( _options, info );

    if( filesystem::assert_dir(_options.repoCachePath) )
    {
      Exception ex(str::form( _("Can't create %s"), _options.repoCachePath.c_str()) );
      ZYPP_THROW(ex);
    }
    RepoStatus raw_metadata_status = metadataStatus(info);
    if ( raw_metadata_status.empty() )
    {
       /* if there is no cache at this point, we refresh the raw
          in case this is the first time - if it's !autorefresh,
          we may still refresh */
      refreshMetadata(info, RefreshIfNeeded, progressrcv );
      raw_metadata_status = metadataStatus(info);
    }

    bool needs_cleaning = false;
    if ( isCached( info ) )
    {
      MIL << info.alias() << " is already cached." << endl;
      RepoStatus cache_status = cacheStatus(info);

      if ( cache_status == raw_metadata_status )
      {
        MIL << info.alias() << " cache is up to date with metadata." << endl;
        if ( policy == BuildIfNeeded )
        {
          // On the fly add missing solv.idx files for bash completion.
          const Pathname & base = solv_path_for_repoinfo( _options, info);
          if ( ! PathInfo(base/"solv.idx").isExist() )
            sat::updateSolvFileIndex( base/"solv" );

          return;
        }
        else {
          MIL << info.alias() << " cache rebuild is forced" << endl;
        }
      }

      needs_cleaning = true;
    }

    ProgressData progress(100);
    callback::SendReport<ProgressReport> report;
    progress.sendTo( ProgressReportAdaptor( progressrcv, report ) );
    progress.name(str::form(_("Building repository '%s' cache"), info.label().c_str()));
    progress.toMin();

    if (needs_cleaning)
    {
      cleanCache(info);
    }

    MIL << info.alias() << " building cache..." << info.type() << endl;

    Pathname base = solv_path_for_repoinfo( _options, info);

    if( filesystem::assert_dir(base) )
    {
      Exception ex(str::form( _("Can't create %s"), base.c_str()) );
      ZYPP_THROW(ex);
    }

    if( ! PathInfo(base).userMayW() )
    {
      Exception ex(str::form( _("Can't create cache at %s - no writing permissions."), base.c_str()) );
      ZYPP_THROW(ex);
    }
    Pathname solvfile = base / "solv";

    // do we have type?
    repo::RepoType repokind = info.type();

    // if the type is unknown, try probing.
    switch ( repokind.toEnum() )
    {
      case RepoType::NONE_e:
        // unknown, probe the local metadata
        repokind = probeCache( productdatapath );
      break;
      default:
      break;
    }

    MIL << "repo type is " << repokind << endl;

    switch ( repokind.toEnum() )
    {
      case RepoType::RPMMD_e :
      case RepoType::YAST2_e :
      case RepoType::RPMPLAINDIR_e :
      {
        // Take care we unlink the solvfile on exception
        ManagedFile guard( solvfile, filesystem::unlink );
        scoped_ptr<MediaMounter> forPlainDirs;

        ExternalProgram::Arguments cmd;
        cmd.push_back( PathInfo( "/usr/bin/repo2solv" ).isFile() ? "repo2solv" : "repo2solv.sh" );
        // repo2solv expects -o as 1st arg!
        cmd.push_back( "-o" );
        cmd.push_back( solvfile.asString() );
        cmd.push_back( "-X" );	// autogenerate pattern from pattern-package
        // bsc#1104415: no more application support // cmd.push_back( "-A" );	// autogenerate application pseudo packages

        if ( repokind == RepoType::RPMPLAINDIR )
        {
          forPlainDirs.reset( new MediaMounter( info.url() ) );
          // recusive for plaindir as 2nd arg!
          cmd.push_back( "-R" );
          // FIXME this does only work form dir: URLs
          cmd.push_back( forPlainDirs->getPathName( info.path() ).c_str() );
        }
        else
          cmd.push_back( productdatapath.asString() );

        ExternalProgram prog( cmd, ExternalProgram::Stderr_To_Stdout );
        std::string errdetail;

        for ( std::string output( prog.receiveLine() ); output.length(); output = prog.receiveLine() ) {
          WAR << "  " << output;
          errdetail += output;
        }

        int ret = prog.close();
        if ( ret != 0 )
        {
          RepoException ex(info, str::form( _("Failed to cache repo (%d)."), ret ));
          ex.addHistory( str::Str() << prog.command() << endl << errdetail << prog.execError() ); // errdetail lines are NL-terminaled!
          ZYPP_THROW(ex);
        }

        // We keep it.
        guard.resetDispose();
        sat::updateSolvFileIndex( solvfile );	// content digest for zypper bash completion
      }
      break;
      default:
        ZYPP_THROW(RepoUnknownTypeException( info, _("Unhandled repository type") ));
      break;
    }
    // update timestamp and checksum
    setCacheStatus(info, raw_metadata_status);
    MIL << "Commit cache.." << endl;
    progress.toMax();
  }

  ////////////////////////////////////////////////////////////////////////////


  /** Probe the metadata type of a repository located at \c url.
   * Urls here may be rewritten by \ref MediaSetAccess to reflect the correct media number.
   *
   * \note Metadata in local cache directories must be probed using \ref probeCache as
   * a cache path must not be rewritten (bnc#946129)
   */
  repo::RepoType RepoManager::Impl::probe( const Url & url, const Pathname & path  ) const
  {
    using namespace zyppng;
    using namespace zyppng::operators;
    using zyppng::operators::operator|;

    auto ctx = zyppng::SyncContext::create();
    auto res = ctx->provider()->attachMedia( url, zyppng::ProvideMediaSpec() )
        | and_then( [&]( auto mediaHandle ) {
          return zyppng::RepoManagerWorkflow::probeRepoType( ctx, mediaHandle, path );
        });

    if ( !res ) {
      ZYPP_RETHROW ( res.error() );
    }
    return res.get();
  }

  ////////////////////////////////////////////////////////////////////////////

  void RepoManager::Impl::loadFromCache( const RepoInfo & info, const ProgressData::ReceiverFnc & progressrcv )
  {
    try
    {
      RepoManagerBaseImpl::loadFromCache( info, progressrcv );
    }
    catch ( const Exception & exp )
    {
      ZYPP_CAUGHT( exp );
      MIL << "Try to handle exception by rebuilding the solv-file" << endl;
      cleanCache( info, progressrcv );
      buildCache( info, BuildIfNeeded, progressrcv );

      sat::Pool::instance().addRepoSolv( solv_path_for_repoinfo(_options, info) / "solv", info );
    }
  }

  ////////////////////////////////////////////////////////////////////////////

  void RepoManager::Impl::addRepository( const RepoInfo & info, const ProgressData::ReceiverFnc & progressrcv )
  {
    assert_alias(info);

    ProgressData progress(100);
    callback::SendReport<ProgressReport> report;
    progress.sendTo( ProgressReportAdaptor( progressrcv, report ) );
    progress.name(str::form(_("Adding repository '%s'"), info.label().c_str()));
    progress.toMin();

    MIL << "Try adding repo " << info << endl;

    RepoInfo tosave = info;
    if ( repos().find(tosave) != repos().end() )
      ZYPP_THROW(RepoAlreadyExistsException(info));

    // check the first url for now
    if ( _options.probe )
    {
      DBG << "unknown repository type, probing" << endl;
      assert_urls(tosave);

      RepoType probedtype( probe( tosave.url(), info.path() ) );
      if ( probedtype == RepoType::NONE )
        ZYPP_THROW(RepoUnknownTypeException(info));
      else
        tosave.setType(probedtype);
    }

    progress.set(50);

    RepoManagerBaseImpl::addProbedRepository ( info, info.type() );

    progress.toMax();
    MIL << "done" << endl;
  }


  void RepoManager::Impl::addRepositories( const Url & url, const ProgressData::ReceiverFnc & progressrcv )
  {
    std::list<RepoInfo> repos = readRepoFile(url);
    for ( std::list<RepoInfo>::const_iterator it = repos.begin();
          it != repos.end();
          ++it )
    {
      // look if the alias is in the known repos.
      for_ ( kit, repoBegin(), repoEnd() )
      {
        if ( (*it).alias() == (*kit).alias() )
        {
          ERR << "To be added repo " << (*it).alias() << " conflicts with existing repo " << (*kit).alias() << endl;
          ZYPP_THROW(RepoAlreadyExistsException(*it));
        }
      }
    }

    std::string filename = Pathname(url.getPathName()).basename();

    if ( filename == Pathname() )
    {
      // TranslatorExplanation '%s' is an URL
      ZYPP_THROW(RepoException(str::form( _("Invalid repo file name at '%s'"), url.asString().c_str() )));
    }

    // assert the directory exists
    filesystem::assert_dir(_options.knownReposPath);

    Pathname repofile = generateNonExistingName(_options.knownReposPath, filename);
    // now we have a filename that does not exists
    MIL << "Saving " << repos.size() << " repo" << ( repos.size() ? "s" : "" ) << " in " << repofile << endl;

    std::ofstream file(repofile.c_str());
    if (!file)
    {
      // TranslatorExplanation '%s' is a filename
      ZYPP_THROW( Exception(str::form( _("Can't open file '%s' for writing."), repofile.c_str() )));
    }

    for ( std::list<RepoInfo>::iterator it = repos.begin();
          it != repos.end();
          ++it )
    {
      MIL << "Saving " << (*it).alias() << endl;
      it->dumpAsIniOn(file);
      it->setFilepath(repofile);
      it->setMetadataPath( rawcache_path_for_repoinfo( _options, *it ) );
      it->setPackagesPath( packagescache_path_for_repoinfo( _options, *it ) );
      reposManip().insert(*it);

      HistoryLog(_options.rootDir).addRepository(*it);
    }

    MIL << "done" << endl;
  }

  void RepoManager::Impl::removeRepository(const RepoInfo &info, const ProgressData::ReceiverFnc &progressrcv )
  {
    callback::SendReport<ProgressReport> report;
    ProgressReportAdaptor adapt( progressrcv, report );
    removeRepositoryImpl( info, std::ref(adapt) );
  }

  ////////////////////////////////////////////////////////////////////////////
  //
  // Services
  //
  ////////////////////////////////////////////////////////////////////////////

  void RepoManager::Impl::refreshServices( const RefreshServiceOptions & options_r )
  {
    // copy the set of services since refreshService
    // can eventually invalidate the iterator
    ServiceSet services( serviceBegin(), serviceEnd() );
    for_( it, services.begin(), services.end() )
    {
      if ( !it->enabled() )
        continue;

      try {
        refreshService(*it, options_r);
      }
      catch ( const repo::ServicePluginInformalException & e )
      { ;/* ignore ServicePluginInformalException */ }
    }
  }

  void RepoManager::Impl::refreshService( const std::string & alias, const RefreshServiceOptions & options_r )
  {
    ServiceInfo service( getService( alias ) );
    assert_alias( service );
    assert_url( service );
    MIL << "Going to refresh service '" << service.alias() <<  "', url: " << service.url() << ", opts: " << options_r << endl;

    if ( service.ttl() && !( options_r.testFlag( RefreshService_forceRefresh) || options_r.testFlag( RefreshService_restoreStatus ) ) )
    {
      // Service defines a TTL; maybe we can re-use existing data without refresh.
      Date lrf = service.lrf();
      if ( lrf )
      {
        Date now( Date::now() );
        if ( lrf <= now )
        {
          if ( (lrf+=service.ttl()) > now ) // lrf+= !
          {
            MIL << "Skip: '" << service.alias() << "' metadata valid until " << lrf << endl;
            return;
          }
        }
        else
          WAR << "Force: '" << service.alias() << "' metadata last refresh in the future: " << lrf << endl;
      }
    }

    // NOTE: It might be necessary to modify and rewrite the service info.
    // Either when probing the type, or when adjusting the repositories
    // enable/disable state.:
    bool serviceModified = false;

    //! \todo add callbacks for apps (start, end, repo removed, repo added, repo changed)?

    // if the type is unknown, try probing.
    if ( service.type() == repo::ServiceType::NONE )
    {
      repo::ServiceType type = probeService( service.url() );
      if ( type != ServiceType::NONE )
      {
        service.setProbedType( type ); // lazy init!
        serviceModified = true;
      }
    }

    // get target distro identifier
    std::string servicesTargetDistro = _options.servicesTargetDistro;
    if ( servicesTargetDistro.empty() )
    {
      servicesTargetDistro = Target::targetDistribution( Pathname() );
    }
    DBG << "ServicesTargetDistro: " << servicesTargetDistro << endl;

    // parse it
    Date::Duration origTtl = service.ttl();	// FIXME Ugly hack: const service.ttl modified when parsing
    RepoCollector collector(servicesTargetDistro);
    // FIXME Ugly hack: ServiceRepos may throw ServicePluginInformalException
    // which is actually a notification. Using an exception for this
    // instead of signal/callback is bad. Needs to be fixed here, in refreshServices()
    // and in zypper.
    std::pair<DefaultIntegral<bool,false>, repo::ServicePluginInformalException> uglyHack;
    try {
      // FIXME bsc#1080693: Shortcoming of (plugin)services (and repos as well) is that they
      // are not aware of the RepoManagers rootDir. The service url, as created in known_services,
      // contains the full path to the script. The script however has to be executed chrooted.
      // Repos would need to know the RepoMangers rootDir to use the correct vars.d to replace
      // repos variables. Until RepoInfoBase is aware if the rootDir, we need to explicitly pass it
      // to ServiceRepos.
      ServiceRepos( _options.rootDir, service, bind( &RepoCollector::collect, &collector, _1 ) );
    }
    catch ( const repo::ServicePluginInformalException & e )
    {
      /* ignore ServicePluginInformalException and throw later */
      uglyHack.first = true;
      uglyHack.second = e;
    }
    if ( service.ttl() != origTtl )	// repoindex.xml changed ttl
    {
      if ( !service.ttl() )
        service.setLrf( Date() );	// don't need lrf when zero ttl
      serviceModified = true;
    }
    ////////////////////////////////////////////////////////////////////////////
    // On the fly remember the new repo states as defined the reopoindex.xml.
    // Move into ServiceInfo later.
    ServiceInfo::RepoStates newRepoStates;

    // set service alias and base url for all collected repositories
    for_( it, collector.repos.begin(), collector.repos.end() )
    {
      // First of all: Prepend service alias:
      it->setAlias( str::form( "%s:%s", service.alias().c_str(), it->alias().c_str() ) );
      // set reference to the parent service
      it->setService( service.alias() );

      // remember the new parsed repo state
      newRepoStates[it->alias()] = *it;

      // - If the repo url was not set by the repoindex parser, set service's url.
      // - Libzypp currently has problem with separate url + path handling so just
      //   append a path, if set, to the baseurls
      // - Credentials in the url authority will be extracted later, either if the
      //   repository is added or if we check for changed urls.
      Pathname path;
      if ( !it->path().empty() )
      {
        if ( it->path() != "/" )
          path = it->path();
        it->setPath("");
      }

      if ( it->baseUrlsEmpty() )
      {
        Url url( service.rawUrl() );
        if ( !path.empty() )
          url.setPathName( url.getPathName() / path );
        it->setBaseUrl( std::move(url) );
      }
      else if ( !path.empty() )
      {
        RepoInfo::url_set urls( it->rawBaseUrls() );
        for ( Url & url : urls )
        {
          url.setPathName( url.getPathName() / path );
        }
        it->setBaseUrls( std::move(urls) );
      }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Now compare collected repos with the ones in the system...
    //
    RepoInfoList oldRepos;
    getRepositoriesInService( service.alias(), std::back_inserter( oldRepos ) );

    ////////////////////////////////////////////////////////////////////////////
    // find old repositories to remove...
    for_( oldRepo, oldRepos.begin(), oldRepos.end() )
    {
      if ( ! foundAliasIn( oldRepo->alias(), collector.repos ) )
      {
        if ( oldRepo->enabled() )
        {
          // Currently enabled. If this was a user modification remember the state.
          const auto & last = service.repoStates().find( oldRepo->alias() );
          if ( last != service.repoStates().end() && ! last->second.enabled )
          {
            DBG << "Service removes user enabled repo " << oldRepo->alias() << endl;
            service.addRepoToEnable( oldRepo->alias() );
            serviceModified = true;
          }
          else
            DBG << "Service removes enabled repo " << oldRepo->alias() << endl;
        }
        else
          DBG << "Service removes disabled repo " << oldRepo->alias() << endl;

        removeRepository( *oldRepo );
      }
    }

    ////////////////////////////////////////////////////////////////////////////
    // create missing repositories and modify existing ones if needed...
    UrlCredentialExtractor urlCredentialExtractor( _options.rootDir );	// To collect any credentials stored in repo URLs
    for_( it, collector.repos.begin(), collector.repos.end() )
    {
      // User explicitly requested the repo being enabled?
      // User explicitly requested the repo being disabled?
      // And hopefully not both ;) If so, enable wins.

      TriBool toBeEnabled( indeterminate );	// indeterminate - follow the service request
      DBG << "Service request to " << (it->enabled()?"enable":"disable") << " service repo " << it->alias() << endl;

      if ( options_r.testFlag( RefreshService_restoreStatus ) )
      {
        DBG << "Opt RefreshService_restoreStatus " << it->alias() << endl;
        // this overrides any pending request!
        // Remove from enable request list.
        // NOTE: repoToDisable is handled differently.
        //       It gets cleared on each refresh.
        service.delRepoToEnable( it->alias() );
        // toBeEnabled stays indeterminate!
      }
      else
      {
        if ( service.repoToEnableFind( it->alias() ) )
        {
          DBG << "User request to enable service repo " << it->alias() << endl;
          toBeEnabled = true;
          // Remove from enable request list.
          // NOTE: repoToDisable is handled differently.
          //       It gets cleared on each refresh.
          service.delRepoToEnable( it->alias() );
          serviceModified = true;
        }
        else if ( service.repoToDisableFind( it->alias() ) )
        {
          DBG << "User request to disable service repo " << it->alias() << endl;
          toBeEnabled = false;
        }
      }

      RepoInfoList::iterator oldRepo( findAlias( it->alias(), oldRepos ) );
      if ( oldRepo == oldRepos.end() )
      {
        // Not found in oldRepos ==> a new repo to add

        // Make sure the service repo is created with the appropriate enablement
        if ( ! indeterminate(toBeEnabled) )
          it->setEnabled( ( bool ) toBeEnabled );

        DBG << "Service adds repo " << it->alias() << " " << (it->enabled()?"enabled":"disabled") << endl;
        addRepository( *it );
      }
      else
      {
        // ==> an exising repo to check
        bool oldRepoModified = false;

        if ( indeterminate(toBeEnabled) )
        {
          // No user request: check for an old user modificaton otherwise follow service request.
          // NOTE: Assert toBeEnabled is boolean afterwards!
          if ( oldRepo->enabled() == it->enabled() )
            toBeEnabled = it->enabled();	// service requests no change to the system
          else if (options_r.testFlag( RefreshService_restoreStatus ) )
          {
            toBeEnabled = it->enabled();	// RefreshService_restoreStatus forced
            DBG << "Opt RefreshService_restoreStatus " << it->alias() <<  " forces " << (toBeEnabled?"enabled":"disabled") << endl;
          }
          else
          {
            const auto & last = service.repoStates().find( oldRepo->alias() );
            if ( last == service.repoStates().end() || last->second.enabled != it->enabled() )
              toBeEnabled = it->enabled();	// service request has changed since last refresh -> follow
            else
            {
              toBeEnabled = oldRepo->enabled();	// service request unchaned since last refresh -> keep user modification
              DBG << "User modified service repo " << it->alias() <<  " may stay " << (toBeEnabled?"enabled":"disabled") << endl;
            }
          }
        }

        // changed enable?
        if ( toBeEnabled == oldRepo->enabled() )
        {
          DBG << "Service repo " << it->alias() << " stays " <<  (oldRepo->enabled()?"enabled":"disabled") << endl;
        }
        else if ( toBeEnabled )
        {
          DBG << "Service repo " << it->alias() << " gets enabled" << endl;
          oldRepo->setEnabled( true );
          oldRepoModified = true;
        }
        else
        {
          DBG << "Service repo " << it->alias() << " gets disabled" << endl;
          oldRepo->setEnabled( false );
          oldRepoModified = true;
        }

        // all other attributes follow the service request:

        // changed name (raw!)
        if ( oldRepo->rawName() != it->rawName() )
        {
          DBG << "Service repo " << it->alias() << " gets new NAME " << it->rawName() << endl;
          oldRepo->setName( it->rawName() );
          oldRepoModified = true;
        }

        // changed autorefresh
        if ( oldRepo->autorefresh() != it->autorefresh() )
        {
          DBG << "Service repo " << it->alias() << " gets new AUTOREFRESH " << it->autorefresh() << endl;
          oldRepo->setAutorefresh( it->autorefresh() );
          oldRepoModified = true;
        }

        // changed priority?
        if ( oldRepo->priority() != it->priority() )
        {
          DBG << "Service repo " << it->alias() << " gets new PRIORITY " << it->priority() << endl;
          oldRepo->setPriority( it->priority() );
          oldRepoModified = true;
        }

        // changed url?
        {
          RepoInfo::url_set newUrls( it->rawBaseUrls() );
          urlCredentialExtractor.extract( newUrls );	// Extract! to prevent passwds from disturbing the comparison below
          if ( oldRepo->rawBaseUrls() != newUrls )
          {
            DBG << "Service repo " << it->alias() << " gets new URLs " << newUrls << endl;
            oldRepo->setBaseUrls( std::move(newUrls) );
            oldRepoModified = true;
          }
        }

        // changed gpg check settings?
        // ATM only plugin services can set GPG values.
        if ( service.type() == ServiceType::PLUGIN )
        {
          TriBool ogpg[3];	// Gpg RepoGpg PkgGpg
          TriBool ngpg[3];
          oldRepo->getRawGpgChecks( ogpg[0], ogpg[1], ogpg[2] );
          it->     getRawGpgChecks( ngpg[0], ngpg[1], ngpg[2] );
#define Z_CHKGPG(I,N)										\
          if ( ! sameTriboolState( ogpg[I], ngpg[I] ) )						\
          {											\
            DBG << "Service repo " << it->alias() << " gets new "#N"Check " << ngpg[I] << endl;	\
            oldRepo->set##N##Check( ngpg[I] );							\
            oldRepoModified = true;								\
          }
          Z_CHKGPG( 0, Gpg );
          Z_CHKGPG( 1, RepoGpg );
          Z_CHKGPG( 2, PkgGpg );
#undef Z_CHKGPG
        }

        // save if modified:
        if ( oldRepoModified )
        {
          modifyRepository( oldRepo->alias(), *oldRepo );
        }
      }
    }

    // Unlike reposToEnable, reposToDisable is always cleared after refresh.
    if ( ! service.reposToDisableEmpty() )
    {
      service.clearReposToDisable();
      serviceModified = true;
    }

    // Remember original service request for next refresh
    if ( service.repoStates() != newRepoStates )
    {
      service.setRepoStates( std::move(newRepoStates) );
      serviceModified = true;
    }

    ////////////////////////////////////////////////////////////////////////////
    // save service if modified: (unless a plugin service)
    if ( service.type() != ServiceType::PLUGIN )
    {
      if ( service.ttl() )
      {
        service.setLrf( Date::now() );	// remember last refresh
        serviceModified =  true;	// or use a cookie file
      }

      if ( serviceModified )
      {
        // write out modified service file.
        modifyService( service.alias(), service );
      }
    }

    if ( uglyHack.first )
    {
      throw( uglyHack.second ); // intentionally not ZYPP_THROW
    }
  }

  ////////////////////////////////////////////////////////////////////////////

  repo::ServiceType RepoManager::Impl::probeService( const Url & url ) const
  {
    try
    {
      MediaSetAccess access(url);
      if ( access.doesFileExist("/repo/repoindex.xml") )
        return repo::ServiceType::RIS;
    }
    catch ( const media::MediaException &e )
    {
      ZYPP_CAUGHT(e);
      // TranslatorExplanation '%s' is an URL
      RepoException enew(str::form( _("Error trying to read from '%s'"), url.asString().c_str() ));
      enew.remember(e);
      ZYPP_THROW(enew);
    }
    catch ( const Exception &e )
    {
      ZYPP_CAUGHT(e);
      // TranslatorExplanation '%s' is an URL
      Exception enew(str::form( _("Unknown error reading from '%s'"), url.asString().c_str() ));
      enew.remember(e);
      ZYPP_THROW(enew);
    }

    return repo::ServiceType::NONE;
  }

  void RepoManager::Impl::refreshGeoIPData ( const RepoInfo::url_set &urls )
  {
    try  {

      if ( !ZConfig::instance().geoipEnabled() ) {
        MIL << "GeoIp disabled via ZConfig, not refreshing the GeoIP information." << std::endl;
        return;
      }

      std::vector<std::string> hosts;
      for ( const auto &baseUrl : urls ) {
        const auto &host = baseUrl.getHost();
        if ( zypp::any_of( ZConfig::instance().geoipHostnames(), [&host]( const auto &elem ){ return ( zypp::str::compareCI( host, elem ) == 0 ); } ) ) {
          hosts.push_back( host );
          break;
        }
      }

      if ( hosts.empty() ) {
        MIL << "No configured geoip URL found, not updating geoip data" << std::endl;
        return;
      }

      const auto &geoIPCache = ZConfig::instance().geoipCachePath();

      if ( filesystem::assert_dir( geoIPCache ) != 0 ) {
        MIL << "Unable to create cache directory for GeoIP." << std::endl;
        return;
      }

      if ( !PathInfo(geoIPCache).userMayRWX() ) {
        MIL << "No access rights for the GeoIP cache directory." << std::endl;
        return;
      }

      // remove all older cache entries
      filesystem::dirForEachExt( geoIPCache, []( const Pathname &dir, const filesystem::DirEntry &entry ){
        if ( entry.type != filesystem::FT_FILE )
          return true;

        PathInfo pi( dir/entry.name );
        auto age = std::chrono::system_clock::now() - std::chrono::system_clock::from_time_t( pi.mtime() );
        if ( age < std::chrono::hours(24) )
          return true;

        MIL << "Removing GeoIP file for " << entry.name << " since it's older than 24hrs." << std::endl;
        filesystem::unlink( dir/entry.name );
        return true;
      });

      // go over all found hostnames
      std::for_each( hosts.begin(), hosts.end(), [ & ]( const std::string &hostname ) {

        // do not query files that are still there
        if ( zypp::PathInfo( geoIPCache / hostname ).isExist() )  {
          MIL << "Skipping GeoIP request for " << hostname << " since a valid cache entry exists." << std::endl;
          return;
        }

        MIL << "Query GeoIP for " << hostname << std::endl;

        zypp::Url url;
        try
        {
          url.setHost(hostname);
          url.setScheme("https");
        }
        catch(const zypp::Exception &e )
        {
          ZYPP_CAUGHT(e);
          MIL << "Ignoring invalid GeoIP hostname: " << hostname << std::endl;
          return;
        }

        MediaSetAccess acc( url );
        zypp::ManagedFile file;
        try {
          // query the file from the server
          file = zypp::ManagedFile (acc.provideOptionalFile("/geoip"), filesystem::unlink );

        } catch ( const zypp::Exception &e  ) {
          ZYPP_CAUGHT(e);
          MIL << "Failed to query GeoIP from hostname: " << hostname << std::endl;
          return;
        }
        if ( !file->empty() ) {

          constexpr auto writeHostToFile = []( const Pathname &fName, const std::string &host ){
            std::ofstream out;
            out.open( fName.asString(), std::ios_base::trunc );
            if ( out.is_open() ) {
              out << host << std::endl;
            } else {
              MIL << "Failed to create/open GeoIP cache file " << fName << std::endl;
            }
          };

          std::string geoipMirror;
          try {
            xml::Reader reader( *file );
            if ( reader.seekToNode( 1, "host" ) ) {
              const auto &str = reader.nodeText().asString();

              // make a dummy URL to ensure the hostname is valid
              zypp::Url testUrl;
              testUrl.setHost(str);
              testUrl.setScheme("https");

              if ( testUrl.isValid() ) {
                MIL << "Storing geoIP redirection: " << hostname << " -> " << str << std::endl;
                geoipMirror = str;
              }

            } else {
              MIL << "No host entry or empty file returned for GeoIP, remembering for 24hrs" << std::endl;
            }
          } catch ( const zypp::Exception &e ) {
            ZYPP_CAUGHT(e);
            MIL << "Empty or invalid GeoIP file, not requesting again for 24hrs" << std::endl;
          }

          writeHostToFile( geoIPCache / hostname, geoipMirror );
        }
      });

    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT(e);
      MIL << "Failed to query GeoIP data." << std::endl;
    }
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : RepoManager
  //
  ///////////////////////////////////////////////////////////////////

  RepoManager::RepoManager( RepoManagerOptions opt )
  : _pimpl( new Impl(std::move(opt)) )
  {}

  RepoManager::~RepoManager()
  {}

  bool RepoManager::repoEmpty() const
  { return _pimpl->repoEmpty(); }

  RepoManager::RepoSizeType RepoManager::repoSize() const
  { return _pimpl->repoSize(); }

  RepoManager::RepoConstIterator RepoManager::repoBegin() const
  { return _pimpl->repoBegin(); }

  RepoManager::RepoConstIterator RepoManager::repoEnd() const
  { return _pimpl->repoEnd(); }

  RepoInfo RepoManager::getRepo( const std::string & alias ) const
  { return _pimpl->getRepo( alias ); }

  bool RepoManager::hasRepo( const std::string & alias ) const
  { return _pimpl->hasRepo( alias ); }

  std::string RepoManager::makeStupidAlias( const Url & url_r )
  {
    std::string ret( url_r.getScheme() );
    if ( ret.empty() )
      ret = "repo-";
    else
      ret += "-";

    std::string host( url_r.getHost() );
    if ( ! host.empty() )
    {
      ret += host;
      ret += "-";
    }

    static Date::ValueType serial = Date::now();
    ret += Digest::digest( Digest::sha1(), str::hexstring( ++serial ) +url_r.asCompleteString() ).substr(0,8);
    return ret;
  }

  RepoStatus RepoManager::metadataStatus( const RepoInfo & info ) const
  { return _pimpl->metadataStatus( info ); }

  RepoManager::RefreshCheckStatus RepoManager::checkIfToRefreshMetadata( const RepoInfo &info, const Url &url, RawMetadataRefreshPolicy policy )
  { return _pimpl->checkIfToRefreshMetadata( info, url, policy ); }

  Pathname RepoManager::metadataPath( const RepoInfo &info ) const
  { return _pimpl->metadataPath( info ); }

  Pathname RepoManager::packagesPath( const RepoInfo &info ) const
  { return _pimpl->packagesPath( info ); }

  void RepoManager::refreshMetadata( const RepoInfo &info, RawMetadataRefreshPolicy policy, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->refreshMetadata( info, policy, progressrcv ); }

  void RepoManager::cleanMetadata( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->cleanMetadata( info, progressrcv ); }

  void RepoManager::cleanPackages( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->cleanPackages( info, progressrcv ); }

  RepoStatus RepoManager::cacheStatus( const RepoInfo &info ) const
  { return _pimpl->cacheStatus( info ); }

  void RepoManager::buildCache( const RepoInfo &info, CacheBuildPolicy policy, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->buildCache( info, policy, progressrcv ); }

  void RepoManager::cleanCache( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->cleanCache( info, progressrcv ); }

  bool RepoManager::isCached( const RepoInfo &info ) const
  { return _pimpl->isCached( info ); }

  void RepoManager::loadFromCache( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->loadFromCache( info, progressrcv ); }

  void RepoManager::cleanCacheDirGarbage( const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->cleanCacheDirGarbage( progressrcv ); }

  repo::RepoType RepoManager::probe( const Url & url, const Pathname & path ) const
  { return _pimpl->probe( url, path ); }

  repo::RepoType RepoManager::probe( const Url & url ) const
  { return _pimpl->probe( url ); }

  void RepoManager::addRepository( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->addRepository( info, progressrcv ); }

  void RepoManager::addRepositories( const Url &url, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->addRepositories( url, progressrcv ); }

  void RepoManager::removeRepository( const RepoInfo & info, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->removeRepository( info, progressrcv ); }

  void RepoManager::modifyRepository( const std::string &alias, const RepoInfo & newinfo, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->modifyRepository( alias, newinfo, progressrcv ); }

  RepoInfo RepoManager::getRepositoryInfo( const std::string &alias, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->getRepositoryInfo( alias ); }

  RepoInfo RepoManager::getRepositoryInfo( const Url & url, const url::ViewOption & urlview, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->getRepositoryInfo( url, urlview ); }

  bool RepoManager::serviceEmpty() const
  { return _pimpl->serviceEmpty(); }

  RepoManager::ServiceSizeType RepoManager::serviceSize() const
  { return _pimpl->serviceSize(); }

  RepoManager::ServiceConstIterator RepoManager::serviceBegin() const
  { return _pimpl->serviceBegin(); }

  RepoManager::ServiceConstIterator RepoManager::serviceEnd() const
  { return _pimpl->serviceEnd(); }

  ServiceInfo RepoManager::getService( const std::string & alias ) const
  { return _pimpl->getService( alias ); }

  bool RepoManager::hasService( const std::string & alias ) const
  { return _pimpl->hasService( alias ); }

  repo::ServiceType RepoManager::probeService( const Url &url ) const
  { return _pimpl->probeService( url ); }

  void RepoManager::addService( const std::string & alias, const Url& url )
  { return _pimpl->addService( alias, url ); }

  void RepoManager::addService( const ServiceInfo & service )
  { return _pimpl->addService( service ); }

  void RepoManager::removeService( const std::string & alias )
  { return _pimpl->removeService( alias ); }

  void RepoManager::removeService( const ServiceInfo & service )
  { return _pimpl->removeService( service ); }

  void RepoManager::refreshServices( const RefreshServiceOptions & options_r )
  { return _pimpl->refreshServices( options_r ); }

  void RepoManager::refreshService( const std::string & alias, const RefreshServiceOptions & options_r )
  { return _pimpl->refreshService( alias, options_r ); }

  void RepoManager::refreshService( const ServiceInfo & service, const RefreshServiceOptions & options_r )
  { return _pimpl->refreshService( service, options_r ); }

  void RepoManager::modifyService( const std::string & oldAlias, const ServiceInfo & service )
  { return _pimpl->modifyService( oldAlias, service ); }

  void RepoManager::refreshGeoIp (const RepoInfo::url_set &urls)
  { return _pimpl->refreshGeoIPData( urls ); }

  ////////////////////////////////////////////////////////////////////////////

  std::ostream & operator<<( std::ostream & str, const RepoManager & obj )
  { return str << *obj._pimpl; }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
