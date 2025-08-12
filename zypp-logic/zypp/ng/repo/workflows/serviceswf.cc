/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "serviceswf.h"


#include <zypp-core/ExternalProgram.h>
#include <zypp-core/base/Gettext.h>
#include <zypp-core/base/LogTools.h>
#include <zypp-core/ng/pipelines/MTry>
#include <zypp-core/ng/pipelines/Await>
#include <zypp-core/ng/io/Process>
#include <zypp-media/ng/providespec.h>
#include <zypp/parser/RepoindexFileReader.h>
#include <zypp/parser/RepoFileReader.h>
#include <zypp/repo/RepoException.h>
#include <zypp/Target.h>
#include <zypp/zypp_detail/urlcredentialextractor_p.h>

#include <zypp/ng/Context>
#include <zypp/ng/media/provide.h>
#include <zypp/ng/workflows/logichelpers.h>

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::repomanager"

namespace zyppng::RepoServicesWorkflow {

  using namespace zyppng::operators;


  namespace {

    zypp::Url adaptServiceUrlToChroot( zypp::Url serviceUrl, zypp::Pathname root ) {
      // if file:// or dir:// path we must prefix with the root_r
      const auto &scheme = serviceUrl.getScheme();
      if ( !root.empty() && (scheme == "dir" || scheme == "file") ) {
        serviceUrl.setPathName ( root / zypp::Pathname(serviceUrl.getPathName()) );
      }
      return serviceUrl;
    }

    template <class Executor, class OpType>
    struct FetchRIMServiceLogic : public LogicBase<Executor, OpType>
    {
      protected:
        ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

      public:
        FetchRIMServiceLogic( ContextRef &&ctx, zypp::Pathname &&root_r, ServiceInfo &&service, ProgressObserverRef &&myProgress )
          : _ctx( std::move(ctx) )
          , _root_r( std::move(root_r) )
          , _service( std::move(service) )
          , _myProgress( std::move(myProgress) )
        {}


        MaybeAsyncRef<expected< std::pair<zypp::ServiceInfo, RepoInfoList> >> execute() {

          using namespace zyppng::operators;

          return zyppng::mtry( [this]{
            // repoindex.xml must be fetched always without using cookies (bnc #573897)
            zypp::Url serviceUrl = _service.url();
            serviceUrl.setQueryParam( "cookies", "0" );
            return adaptServiceUrlToChroot( serviceUrl, _root_r );
          })
          | and_then( [this]( zypp::Url serviceUrl ){ return _ctx->provider()->attachMedia( serviceUrl, ProvideMediaSpec() ); })
          | and_then( [this]( auto mediaHandle )    { return _ctx->provider()->provide( mediaHandle, "repo/repoindex.xml", ProvideFileSpec() ); } )
          | and_then( [this]( auto provideResult )  {
            try {

              zypp::RepoInfoList repos;
              auto callback = [&]( const zypp::RepoInfo &r) { repos.push_back(r); return true; };

              zypp::parser::RepoindexFileReader reader( provideResult.file(), callback);
              _service.setProbedTtl( reader.ttl() );	// hack! Modifying the const Service to set parsed TTL

              return make_expected_success( std::make_pair( _service, std::move(repos) ) );

            } catch ( const zypp::Exception &e ) {
              //Reader throws a bare exception, we need to translate it into something our calling
              //code expects and handles (bnc#1116840)
              ZYPP_CAUGHT ( e );
              zypp::repo::ServicePluginInformalException ex ( e.msg() );
              ex.remember( e );
              return expected<std::pair<zypp::ServiceInfo, RepoInfoList>>::error( ZYPP_EXCPT_PTR( ex ) );
            }
          });
        }

    private:
         ContextRef  _ctx;
         zypp::Pathname      _root_r;
         ServiceInfo         _service;
         ProgressObserverRef _myProgress;
    };


    template <class Executor, class OpType>
    struct FetchPluginServiceLogic : public LogicBase<Executor, OpType>
    {
      protected:
      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

      public:
        using Ret = expected<std::pair<zypp::ServiceInfo, RepoInfoList>>;

        FetchPluginServiceLogic( ContextRef &&ctx, zypp::Pathname &&root_r, ServiceInfo &&service, ProgressObserverRef &&myProgress )
          : _ctx( std::move(ctx) )
          , _root_r( std::move(root_r) )
          , _service( std::move(service) )
          , _myProgress( std::move(myProgress) )
        {}


        MaybeAsyncRef<Ret> execute() {
          using namespace zyppng::operators;

          // bsc#1080693: Service script needs to be executed chrooted to the RepoManagers rootDir.
          // The service is not aware of the rootDir, so it's explicitly passed and needs to be
          // stripped from the URLs path.
          auto stripped = zypp::Pathname::stripprefix( _root_r, _service.url().getPathName() ).asString();

          return executor()->runPlugin( std::move(stripped) )
          | and_then( [this]( int exitCode ) {

            if ( exitCode != 0 ) {
              // ServicePluginInformalException:
              // Ignore this error but we'd like to report it somehow...
              ERR << "Capture plugin error:[" << std::endl << _stderrBuf << std::endl << ']' << std::endl;
              return Ret::error( ZYPP_EXCPT_PTR( zypp::repo::ServicePluginInformalException( _service, _stderrBuf ) ) );
            }

            try {
              zypp::RepoInfoList repos;
              auto callback = [&]( const zypp::RepoInfo &r) { repos.push_back(r); return true; };

              std::stringstream buffer( _stdoutBuf );
              zypp::parser::RepoFileReader parser( buffer, callback );
              return make_expected_success( std::make_pair( _service, std::move(repos) ) );

            } catch (...) {
              return Ret::error( std::current_exception () );
            }
          });
        }
      private:

#ifdef ZYPP_ENABLE_ASYNC
        AsyncOpRef<expected<int>> runPlugin( std::string command ) {
          using namespace zyppng::operators;

          const char *args[] = {
            "/bin/sh",
            "-c",
            command.c_str(),
            nullptr
          };

          auto pluginProcess = Process::create();
          pluginProcess->setChroot ( _root_r );

          // make sure our process is actually running, if not finalize right away
          if ( !pluginProcess->start( args ) || !pluginProcess->isRunning () ) {
            return makeReadyResult ( finalize( std::move(pluginProcess) ) );
          }

          return std::move(pluginProcess)
            | await<Process>( &Process::sigFinished ) // wait for finished sig
            | [this]( ProcessRef proc ) { return finalize( std::move(proc) ); };
        }

        expected<int> finalize( ProcessRef proc ) {
          if ( proc->isRunning () ) {
            proc->stop ( SIGKILL );
            return expected<int>::error( ZYPP_EXCPT_PTR( zypp::Exception("Bug, plugin process was still running after receiving sigFinished")) );
          }

          _stdoutBuf = proc->readAll( Process::StdOut ).asString();
          if ( proc->exitStatus() != 0 ) {
            _stderrBuf = proc->readAll( Process::StdErr ).asString();
          }

          return make_expected_success ( proc->exitStatus () );
        }
#else
        expected<int> runPlugin( std::string command ) {
          try {
            std::stringstream buffer;

            zypp::ExternalProgram::Arguments args;
            args.reserve( 3 );
            args.push_back( "/bin/sh" );
            args.push_back( "-c" );
            args.push_back( command );

            zypp::ExternalProgramWithStderr prog( args, _root_r );
            prog >> buffer;
            _stdoutBuf = buffer.str();

            int retCode = prog.close();
            if ( retCode != 0 ) {
              // ServicePluginInformalException:
              // Ignore this error but we'd like to report it somehow...
              prog.stderrGetUpTo( _stderrBuf, '\0' );
            }
            return make_expected_success(retCode);
          } catch ( ... ) {
            return expected<int>::error( ZYPP_FWD_CURRENT_EXCPT() );
          }
        }
#endif


         ContextRef  _ctx;
         zypp::Pathname      _root_r;
         ServiceInfo         _service;
         ProgressObserverRef _myProgress;
         std::string         _stdoutBuf;
         std::string         _stderrBuf;
    };


    struct SyncFetchPluginService : FetchPluginServiceLogic<SyncFetchPluginService, SyncOp< expected< std::pair<zypp::ServiceInfo, RepoInfoList> >>>
    {
      using FetchPluginServiceLogic::FetchPluginServiceLogic;

    };

    struct ASyncFetchPluginService : FetchPluginServiceLogic<ASyncFetchPluginService, AsyncOp< expected< std::pair<zypp::ServiceInfo, RepoInfoList> >>>
    {
      using FetchPluginServiceLogic::FetchPluginServiceLogic;

    };
  }

  MaybeAwaitable<expected<std::pair<zypp::ServiceInfo, RepoInfoList>>> fetchRepoListfromService( ContextRef ctx, zypp::Pathname root_r, ServiceInfo service, ProgressObserverRef myProgress )
  {
    if constexpr ( ZYPP_IS_ASYNC ) {
      if ( service.type() == zypp::repo::ServiceType::PLUGIN )
        return SimpleExecutor<FetchPluginServiceLogic, AsyncOp<expected< std::pair<zypp::ServiceInfo, RepoInfoList> >>>::run( std::move(ctx), std::move(root_r), std::move(service), std::move(myProgress) );
      else
        return SimpleExecutor<FetchRIMServiceLogic, AsyncOp<expected< std::pair<zypp::ServiceInfo, RepoInfoList> >>>::run( std::move(ctx), std::move(root_r), std::move(service), std::move(myProgress) );
    } else {
      if ( service.type() == zypp::repo::ServiceType::PLUGIN )
        return SimpleExecutor<FetchPluginServiceLogic, SyncOp<expected< std::pair<zypp::ServiceInfo, RepoInfoList> >>>::run( std::move(ctx), std::move(root_r), std::move(service), std::move(myProgress) );
      else
        return SimpleExecutor<FetchRIMServiceLogic, SyncOp<expected< std::pair<zypp::ServiceInfo, RepoInfoList> >>>::run( std::move(ctx), std::move(root_r), std::move(service), std::move(myProgress) );
    }
  }

  namespace {

    auto probeServiceLogic( ContextRef ctx, const zypp::Url &url ) {

      using MediaHandle = ProvideMediaHandle;
      using ProvideRes  = zyppng::ProvideRes;

      return ctx->provider()->attachMedia( url, ProvideMediaSpec() )
      | and_then( [ctx]( MediaHandle medium ) { return ctx->provider()->provide( medium, "/repo/repoindex.xml", ProvideFileSpec().setCheckExistsOnly()); } )
      | [url]( expected<ProvideRes> result ) {
        if ( result )
          return expected<zypp::repo::ServiceType>::success( zypp::repo::ServiceType::RIS );

        try{
          std::rethrow_exception( result.error() );
        } catch ( const zypp::media::MediaFileNotFoundException &e ) {
          // fall through
        } catch ( const zypp::media::MediaException &e ) {
          ZYPP_CAUGHT(e);
          // TranslatorExplanation '%s' is an URL
          zypp::repo::RepoException enew(zypp::str::form( _("Error trying to read from '%s'"), url.asString().c_str() ));
          enew.remember(e);
          return expected<zypp::repo::ServiceType>::error( ZYPP_EXCPT_PTR(enew) );
        }
        catch ( const zypp::Exception &e ) {
          ZYPP_CAUGHT(e);
          // TranslatorExplanation '%s' is an URL
          zypp::Exception enew(zypp::str::form( _("Unknown error reading from '%s'"), url.asString().c_str() ));
          enew.remember(e);
          return expected<zypp::repo::ServiceType>::error( ZYPP_EXCPT_PTR(enew) );
        }
        catch ( ... ) {
          // TranslatorExplanation '%s' is an URL
          zypp::Exception enew(zypp::str::form( _("Unknown error reading from '%s'"), url.asString().c_str() ));
          enew.remember( std::current_exception() );
          return expected<zypp::repo::ServiceType>::error( ZYPP_EXCPT_PTR(enew) );
        }

        return expected<zypp::repo::ServiceType>::success( zypp::repo::ServiceType::NONE );
      };
    }
  }

  MaybeAwaitable<expected<zypp::repo::ServiceType> > probeServiceType(ContextRef ctx, const zypp::Url &url)
  {
    if constexpr ( ZYPP_IS_ASYNC )
      return probeServiceLogic( std::move(ctx), url );
    else
      return probeServiceLogic( std::move(ctx), url );
  }

  namespace {
    template <class Executor, class OpType>
    struct RefreshServiceLogic : public LogicBase<Executor, OpType>
    {
    protected:
      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

    public:
      using Ret = expected<void>;

      RefreshServiceLogic( RepoManagerRef &&repoMgr, zypp::ServiceInfo &&info, RepoManager::RefreshServiceOptions options )
        : _repoMgr( std::move(repoMgr) )
        , _service( std::move(info) )
        , _options(options)
      { }

      MaybeAsyncRef<expected<void>> probeServiceIfNeeded() {
        // if the type is unknown, try probing.
        if ( _service.type() == zypp::repo::ServiceType::NONE ) {

          return probeServiceType( _repoMgr->zyppContext(), adaptServiceUrlToChroot( _service.url(), _repoMgr->options().rootDir ) )
          | and_then( [this]( zypp::repo::ServiceType type ){
              _service.setProbedType( type ); // lazy init!
              _serviceModified = true;
              return expected<void>::success();
          } );

        }
        return makeReadyResult( expected<void>::success() );
      }

      MaybeAsyncRef<Ret> execute() {

        try {
          assert_alias( _service ).unwrap();
          assert_url( _service ).unwrap();
        } catch (...) {
          return makeReadyResult(Ret::error(ZYPP_FWD_CURRENT_EXCPT()));
        }

        MIL << "Going to refresh service '" << _service.alias() <<  "', url: " << _service.url() << ", opts: " << _options << std::endl;

        if ( _service.ttl() && !( _options.testFlag( zypp::RepoManagerFlags::RefreshService_forceRefresh) || _options.testFlag( zypp::RepoManagerFlags::RefreshService_restoreStatus ) ) )
        {
          // Service defines a TTL; maybe we can re-use existing data without refresh.
          zypp::Date lrf = _service.lrf();
          if ( lrf )
          {
            zypp::Date now( zypp::Date::now() );
            if ( lrf <= now )
            {
              if ( (lrf+=_service.ttl()) > now ) // lrf+= !
              {
                MIL << "Skip: '" << _service.alias() << "' metadata valid until " << lrf << std::endl;
                return makeReadyResult( Ret::success() );
              }
            }
            else
              WAR << "Force: '" << _service.alias() << "' metadata last refresh in the future: " << lrf << std::endl;
          }
        }

        //! \todo add callbacks for apps (start, end, repo removed, repo added, repo changed)?

        return probeServiceIfNeeded () // if the type is unknown, try probing.
        | and_then( [this]() {
          // FIXME bsc#1080693: Shortcoming of (plugin)services (and repos as well) is that they
          // are not aware of the RepoManagers rootDir. The service url, as created in known_services,
          // contains the full path to the script. The script however has to be executed chrooted.
          // Repos would need to know the RepoMangers rootDir to use the correct vars.d to replace
          // repos variables. Until RepoInfoBase is aware if the rootDir, we need to explicitly pass it
          // to ServiceRepos.
          return fetchRepoListfromService( _repoMgr->zyppContext(), _repoMgr->options().rootDir, _service, nullptr );
          } )
        | [this]( expected<std::pair<zypp::ServiceInfo, RepoInfoList>> serviceReposExp ) {

          if ( !serviceReposExp ) {
            try {
              std::rethrow_exception( serviceReposExp.error() );

            } catch ( const zypp::repo::ServicePluginInformalException & e ) {
              /* ignore ServicePluginInformalException and throw later */
              _informalError = e;
            } catch ( ... ) {
              // all other errors cancel the operation
              return Ret::error( ZYPP_FWD_CURRENT_EXCPT() );
            }
          }

          std::pair<zypp::ServiceInfo, RepoInfoList> serviceRepos = serviceReposExp.is_valid() ? std::move( serviceReposExp.get() ) : std::make_pair(  _service, RepoInfoList{} );

          // get target distro identifier
          std::string servicesTargetDistro = _repoMgr->options().servicesTargetDistro;
          if ( servicesTargetDistro.empty() ) {
            servicesTargetDistro = zypp::Target::targetDistribution( zypp::Pathname() );
          }
          DBG << "ServicesTargetDistro: " << servicesTargetDistro << std::endl;

          // filter repos by target distro
          RepoCollector collector( servicesTargetDistro );
          std::for_each( serviceRepos.second.begin(), serviceRepos.second.end(), [&]( const auto &r ){ collector.collect(r); } );

          if ( _service.ttl () != serviceRepos.first.ttl () ) {
            // repoindex.xml changed ttl
            if ( !serviceRepos.first.ttl() )
              serviceRepos.first.setLrf( zypp::Date() ); // don't need lrf when zero ttl

            _serviceModified = true;
          }

          // service was maybe updated
          _service = serviceRepos.first;

          ////////////////////////////////////////////////////////////////////////////
          // On the fly remember the new repo states as defined the reopoindex.xml.
          // Move into ServiceInfo later.
          ServiceInfo::RepoStates newRepoStates;

          // set service alias and base url for all collected repositories
          for_( it, collector.repos.begin(), collector.repos.end() )
          {
            // First of all: Prepend service alias:
            it->setAlias( zypp::str::form( "%s:%s", _service.alias().c_str(), it->alias().c_str() ) );
            // set reference to the parent service
            it->setService( _service.alias() );

            // remember the new parsed repo state
            newRepoStates[it->alias()] = *it;

            // - If the repo url was not set by the repoindex parser, set service's url.
            // - Libzypp currently has problem with separate url + path handling so just
            //   append a path, if set, to the baseurls
            // - Credentials in the url authority will be extracted later, either if the
            //   repository is added or if we check for changed urls.
            zypp::Pathname path;
            if ( !it->path().empty() )
            {
              if ( it->path() != "/" )
                path = it->path();
              it->setPath("");
            }

            if ( it->baseUrlsEmpty() )
            {
              zypp::Url url( _service.rawUrl() );
              if ( !path.empty() )
                url.setPathName( url.getPathName() / path );
              it->setBaseUrl( std::move(url) );
            }
            else if ( !path.empty() )
            {
              RepoInfo::url_set urls( it->rawBaseUrls() );
              for ( zypp::Url & url : urls )
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
          _repoMgr->getRepositoriesInService( _service.alias(), std::back_inserter( oldRepos ) );

          ////////////////////////////////////////////////////////////////////////////
          // find old repositories to remove...
          for_( oldRepo, oldRepos.begin(), oldRepos.end() )
          {
            if ( ! foundAliasIn( oldRepo->alias(), collector.repos ) )
            {
              if ( oldRepo->enabled() )
              {
                // Currently enabled. If this was a user modification remember the state.
                const auto & last = _service.repoStates().find( oldRepo->alias() );
                if ( last != _service.repoStates().end() && ! last->second.enabled )
                {
                  DBG << "Service removes user enabled repo " << oldRepo->alias() << std::endl;
                  _service.addRepoToEnable( oldRepo->alias() );
                  _serviceModified = true;
                }
                else
                  DBG << "Service removes enabled repo " << oldRepo->alias() << std::endl;
              }
              else
                DBG << "Service removes disabled repo " << oldRepo->alias() << std::endl;

              auto remRes = _repoMgr->removeRepository( *oldRepo );
              if ( !remRes ) return Ret::error( remRes.error() );
            }
          }


          ////////////////////////////////////////////////////////////////////////////
          // create missing repositories and modify existing ones if needed...
          zypp::UrlCredentialExtractor urlCredentialExtractor( _repoMgr->options().rootDir );	// To collect any credentials stored in repo URLs
          for_( it, collector.repos.begin(), collector.repos.end() )
          {
            // User explicitly requested the repo being enabled?
            // User explicitly requested the repo being disabled?
            // And hopefully not both ;) If so, enable wins.

            zypp::TriBool toBeEnabled( zypp::indeterminate );	// indeterminate - follow the service request
            DBG << "Service request to " << (it->enabled()?"enable":"disable") << " service repo " << it->alias() << std::endl;

            if ( _options.testFlag( zypp::RepoManagerFlags::RefreshService_restoreStatus ) )
            {
              DBG << "Opt RefreshService_restoreStatus " << it->alias() << std::endl;
              // this overrides any pending request!
              // Remove from enable request list.
              // NOTE: repoToDisable is handled differently.
              //       It gets cleared on each refresh.
              _service.delRepoToEnable( it->alias() );
              // toBeEnabled stays indeterminate!
            }
            else
            {
              if ( _service.repoToEnableFind( it->alias() ) )
              {
                DBG << "User request to enable service repo " << it->alias() << std::endl;
                toBeEnabled = true;
                // Remove from enable request list.
                // NOTE: repoToDisable is handled differently.
                //       It gets cleared on each refresh.
                _service.delRepoToEnable( it->alias() );
                _serviceModified = true;
              }
              else if ( _service.repoToDisableFind( it->alias() ) )
              {
                DBG << "User request to disable service repo " << it->alias() << std::endl;
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

              DBG << "Service adds repo " << it->alias() << " " << (it->enabled()?"enabled":"disabled") << std::endl;
              const auto &addRes = _repoMgr->addRepository( *it );
              if (!addRes) return Ret::error( addRes.error() );
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
                else if ( _options.testFlag( zypp::RepoManagerFlags::RefreshService_restoreStatus ) )
                {
                  toBeEnabled = it->enabled();	// RefreshService_restoreStatus forced
                  DBG << "Opt RefreshService_restoreStatus " << it->alias() <<  " forces " << (toBeEnabled?"enabled":"disabled") << std::endl;
                }
                else
                {
                  const auto & last = _service.repoStates().find( oldRepo->alias() );
                  if ( last == _service.repoStates().end() || last->second.enabled != it->enabled() )
                    toBeEnabled = it->enabled();	// service request has changed since last refresh -> follow
                  else
                  {
                    toBeEnabled = oldRepo->enabled();	// service request unchaned since last refresh -> keep user modification
                    DBG << "User modified service repo " << it->alias() <<  " may stay " << (toBeEnabled?"enabled":"disabled") << std::endl;
                  }
                }
              }

              // changed enable?
              if ( toBeEnabled == oldRepo->enabled() )
              {
                DBG << "Service repo " << it->alias() << " stays " <<  (oldRepo->enabled()?"enabled":"disabled") << std::endl;
              }
              else if ( toBeEnabled )
              {
                DBG << "Service repo " << it->alias() << " gets enabled" << std::endl;
                oldRepo->setEnabled( true );
                oldRepoModified = true;
              }
              else
              {
                DBG << "Service repo " << it->alias() << " gets disabled" << std::endl;
                oldRepo->setEnabled( false );
                oldRepoModified = true;
              }

              // all other attributes follow the service request:

              // changed name (raw!)
              if ( oldRepo->rawName() != it->rawName() )
              {
                DBG << "Service repo " << it->alias() << " gets new NAME " << it->rawName() << std::endl;
                oldRepo->setName( it->rawName() );
                oldRepoModified = true;
              }

              // changed autorefresh
              if ( oldRepo->autorefresh() != it->autorefresh() )
              {
                DBG << "Service repo " << it->alias() << " gets new AUTOREFRESH " << it->autorefresh() << std::endl;
                oldRepo->setAutorefresh( it->autorefresh() );
                oldRepoModified = true;
              }

              // changed priority?
              if ( oldRepo->priority() != it->priority() )
              {
                DBG << "Service repo " << it->alias() << " gets new PRIORITY " << it->priority() << std::endl;
                oldRepo->setPriority( it->priority() );
                oldRepoModified = true;
              }

              // changed url?
              {
                RepoInfo::url_set newUrls( it->rawBaseUrls() );
                urlCredentialExtractor.extract( newUrls );	// Extract! to prevent passwds from disturbing the comparison below
                if ( oldRepo->rawBaseUrls() != newUrls )
                {
                  DBG << "Service repo " << it->alias() << " gets new URLs " << newUrls << std::endl;
                  oldRepo->setBaseUrls( std::move(newUrls) );
                  oldRepoModified = true;
                }
              }

              // changed gpg check settings?
              // ATM only plugin services can set GPG values.
              if ( _service.type() == zypp::repo::ServiceType::PLUGIN )
              {
                zypp::TriBool ogpg[3];	// Gpg RepoGpg PkgGpg
                zypp::TriBool ngpg[3];
                oldRepo->getRawGpgChecks( ogpg[0], ogpg[1], ogpg[2] );
                it->     getRawGpgChecks( ngpg[0], ngpg[1], ngpg[2] );
      #define Z_CHKGPG(I,N)										\
                if ( ! sameTriboolState( ogpg[I], ngpg[I] ) )						\
                {											\
                  DBG << "Service repo " << it->alias() << " gets new "#N"Check " << ngpg[I] << std::endl;	\
                  oldRepo->set##N##Check( ngpg[I] );							\
                  oldRepoModified = true;								\
                }
                Z_CHKGPG( 0, Gpg );
                Z_CHKGPG( 1, RepoGpg );
                Z_CHKGPG( 2, PkgGpg );
      #undef Z_CHKGPG
              }

              // changed gpgkey settings?
              if ( oldRepo->rawGpgKeyUrls() != it->rawGpgKeyUrls() )
              {
                DBG << "Service repo " << it->alias() << " gets new GPGKEY url " << it->rawGpgKeyUrls() << std::endl;
                oldRepo->setGpgKeyUrls( it->rawGpgKeyUrls() );
                oldRepoModified = true;
              }

              // changed mirrorlist settings?
              if ( oldRepo->rawCfgMirrorlistUrl() != it->rawCfgMirrorlistUrl() )
              {
                DBG << "Service repo " << it->alias() << " gets new MIRRORLIST url " << it->rawCfgMirrorlistUrl() << std::endl;
                oldRepo->setMirrorlistUrl( it->rawCfgMirrorlistUrl() );
                oldRepoModified = true;
              }

              // changed metalink settings?
              if ( oldRepo->rawCfgMetalinkUrl() != it->rawCfgMetalinkUrl() )
              {
                DBG << "Service repo " << it->alias() << " gets new METALINK url " << it->rawCfgMetalinkUrl() << std::endl;
                oldRepo->setMetalinkUrl( it->rawCfgMetalinkUrl() );
                oldRepoModified = true;
              }

              // save if modified:
              if ( oldRepoModified )
              {
                auto modRes = _repoMgr->modifyRepository( oldRepo->alias(), *oldRepo );
                if ( !modRes ) return Ret::error( modRes.error() );
              }
            }
          }

          // Unlike reposToEnable, reposToDisable is always cleared after refresh.
          if ( ! _service.reposToDisableEmpty() )
          {
            _service.clearReposToDisable();
            _serviceModified = true;
          }

          // Remember original service request for next refresh
          if ( _service.repoStates() != newRepoStates )
          {
            _service.setRepoStates( std::move(newRepoStates) );
            _serviceModified = true;
          }

          ////////////////////////////////////////////////////////////////////////////
          // save service if modified: (unless a plugin service)
          if ( _service.type() != zypp::repo::ServiceType::PLUGIN )
          {
            if ( _service.ttl() )
            {
              _service.setLrf( zypp::Date::now() );	// remember last refresh
              _serviceModified =  true;	// or use a cookie file
            }

            if ( _serviceModified )
            {
              // write out modified service file.
              auto modRes = _repoMgr->modifyService( _service.alias(), _service );
              if ( !modRes ) return Ret::error( modRes.error() );
            }
          }

          if ( _informalError ) {
            return Ret::error( std::make_exception_ptr (_informalError.value()) );
          }

          return Ret::success( );
        };
      }


      RepoManagerRef    _repoMgr;
      zypp::ServiceInfo _service;
      RepoManager::RefreshServiceOptions _options;

      // NOTE: It might be necessary to modify and rewrite the service info.
      // Either when probing the type, or when adjusting the repositories
      // enable/disable state.:
      bool _serviceModified = false;

      // FIXME Ugly hack: ServiceRepos may throw ServicePluginInformalException
      // which is actually a notification. Using an exception for this
      // instead of signal/callback is bad. Needs to be fixed here, in refreshServices()
      // and in zypper.
      std::optional<zypp::repo::ServicePluginInformalException> _informalError;
    };
  }

  MaybeAwaitable<expected<void>> refreshService( RepoManagerRef repoMgr, ServiceInfo info, zypp::RepoManagerFlags::RefreshServiceOptions options)
  {
    if constexpr ( ZYPP_IS_ASYNC )
      return SimpleExecutor<RefreshServiceLogic, AsyncOp<expected<void>>>::run( std::move(repoMgr), std::move(info), std::move(options) );
    else
      return SimpleExecutor<RefreshServiceLogic, SyncOp<expected<void>>>::run( std::move(repoMgr), std::move(info), std::move(options) );
  }

}
