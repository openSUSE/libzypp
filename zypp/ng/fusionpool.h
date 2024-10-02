/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_FUSIONPOOL_INCLUDED
#define ZYPP_NG_FUSIONPOOL_INCLUDED

#include <zypp/base/Env.h>
#include <zypp/Resolver.h>
#include <zypp/target/TargetImpl.h>
#include <zypp/Repository.h>
#include <zypp/ResPool.h>
#include <zypp/ZYppCommitResult.h>
#include <zypp/ZYppCommitPolicy.h>

#include <solv/solvversion.h>


#include <zypp/ng/repomanager.h>
#include <zypp-core/zyppng/pipelines/expected.h>
#include <zypp-core/zyppng/base/Base>
#include <zypp-core/zyppng/pipelines/MTry>
#include <zypp-core/zyppng/pipelines/Transform>
#include <zypp-core/zyppng/ui/ProgressObserver>

namespace zyppng {

  ZYPP_FWD_DECL_TEMPL_TYPE_WITH_REFS_ARG1 (FusionPool, ContextType );

  /*!
   * This class is the C++ representation of the GLib-C Pool class,
   * it defines the combination of Repositories, Target and solv Pool that
   * can be used to query and manipulate a system.
   *
   * The zypp context of a repository can be different than the one from
   * the target. Idea here is that we can use the repo config from "/" to install
   * into a target in a different location, with different settings .. e.g. "/mnt/newtarget".
   * The FusionPool will then use the "/" context to download files but install them into "/mnt/newtarget".
   *
   * \note Currently there is just one FusionPool instance, until we figured out if we can refactor the code to support multiple.
   *
   * \sa ZyppPool
   */
  template <typename ContextType>
  class FusionPool : public Base
  {
    ZYPP_ADD_PRIVATE_CONSTR_HELPER ();

    friend class RepoManager<Ref<ContextType>>;

  public:

    FusionPool( ZYPP_PRIVATE_CONSTR_ARG )
      : _pool( zypp::ResPool::instance() )
      , _satPool( zypp::sat::Pool::instance() )
      , _proxy( _pool.proxy() )
      , _resolver( new zypp::Resolver( zypp::ResPool::instance()) )
    { }

    /*!
     * Loads the \ref zypp::Target into the pool, will unload any other currently loaded target before.
     * If \a context is a nullptr just unloads the current target
     */
    expected<void> setTargetContext( Ref<ContextType> context ) {
      try {
        // do nothing if we have the target loaded already
        if ( _targetContext == context ) {
          MIL << "Repeated call to setTargetContext()" << std::endl;
          return expected<void>::success();
        }

        unloadTargetContext ();

        if ( context ) {
          zypp::Target_Ptr newTarget = context->target();
          if ( !newTarget ) {
            ZYPP_THROW(zypp::Exception("Target not initialized."));
          }

          newTarget->buildCache();
          newTarget->load();

          _targetContext   = std::move(context);
          _targetCloseConn = _targetContext->sigClose().connect( sigc::mem_fun( *this, &FusionPool<ContextType>::onContextClose ) );
        }

        return expected<void>::success();

      } catch (...) {
        return expected<void>::error( ZYPP_FWD_CURRENT_EXCPT () );
      }
    }

    void unloadTargetContext () {
      _targetCloseConn.disconnect();
      if (!_targetContext )
        return;

      MIL << "Unloading closing context";
      _targetContext->target()->unload();
      _targetContext.reset();
      _resolver->setDefaultSolverFlags( /*all_r*/false );  // just changed defaults
    }

    Ref<ContextType> targetContext() const { return _targetContext; }

    expected<zypp::ZYppCommitResult> commit (  const zypp::ZYppCommitPolicy & policy_r  ) {
      try {

        if ( getenv("ZYPP_TESTSUITE_FAKE_ARCH") ) {
          ZYPP_THROW( zypp::Exception("ZYPP_TESTSUITE_FAKE_ARCH set. Commit not allowed and disabled.") );
        }

        MIL << "Attempt to commit (" << policy_r << ")" << std::endl;
        if (! _targetContext )
          ZYPP_THROW( zypp::Exception("No target context associated with the pool, commit not possible.") );

        zypp::Target_Ptr _target = _targetContext->target();
        if (! _target)
          ZYPP_THROW( zypp::Exception("Target not initialized.") );

        zypp::env::ScopedSet ea { "ZYPP_IS_RUNNING", zypp::str::numstring(getpid()).c_str() };
        zypp::env::ScopedSet eb;
        if ( _target->chrooted() )
          eb = zypp::env::ScopedSet( "SYSTEMD_OFFLINE", "1" );	// bsc#1118758 - indicate no systemd if chrooted install

        zypp::ZYppCommitResult res = _target->_pimpl->commit( resPool(), policy_r );

        if (! policy_r.dryRun() )
        {
          if ( policy_r.syncPoolAfterCommit() )
            {
              // reload new status from target
              DBG << "reloading " << _satPool.systemRepoAlias() << " repo to pool" << std::endl;
              _target->load();
            }
          else
            {
              DBG << "unloading " << _satPool.systemRepoAlias() << " repo from pool" << std::endl;
              unloadTargetContext();
            }
        }

        MIL << "Commit (" << policy_r << ") returned: "
            << res << std::endl;
        return expected<zypp::ZYppCommitResult>::success(res);
      } catch (...) {
        return expected<zypp::ZYppCommitResult>::error( ZYPP_FWD_CURRENT_EXCPT() );
      }
    }

    expected<void> loadFromCache ( const RepoInfo & info, ProgressObserverRef myProgress ) {
      using namespace zyppng::operators;
      return zyppng::mtry( [this, info, myProgress](){
        ProgressObserver::setup( myProgress, _("Loading from cache"), 3 );
        ProgressObserver::start( myProgress );

        if ( info.solvCachePath ().empty () ) {
          ZYPP_THROW( zypp::Exception("RepoInfo must have a solv cache path initialized") );
        }

        assert_alias(info).unwrap();
        zypp::Pathname solvfile = info.solvCachePath() / "solv";

        if ( ! zypp::PathInfo(solvfile).isExist() )
          ZYPP_THROW(zypp::repo::RepoNotCachedException(info));

        satPool().reposErase( info.alias() );

        ProgressObserver::increase ( myProgress );

        zypp::Repository repo = satPool().addRepoSolv( solvfile, info );

        ProgressObserver::increase ( myProgress );

        // test toolversion in order to rebuild solv file in case
        // it was written by a different libsolv-tool parser.
        const std::string & toolversion( zypp::sat::LookupRepoAttr( zypp::sat::SolvAttr::repositoryToolVersion, repo ).begin().asString() );
        if ( toolversion != LIBSOLV_TOOLVERSION ) {
          repo.eraseFromPool();
          ZYPP_THROW(zypp::Exception(zypp::str::Str() << "Solv-file was created by '"<<toolversion<<"'-parser (want "<<LIBSOLV_TOOLVERSION<<")."));
        }
      })
      /*
      | or_else( [this, repoMgr, info, myProgress]( std::exception_ptr exp ) {
        ZYPP_CAUGHT( exp );
        MIL << "Try to handle exception by rebuilding the solv-file" << std::endl;
        return repoMgr->cleanCache( info, ProgressObserver::makeSubTask( myProgress ) )
          | and_then([this, repoMgr, info, myProgress]{
            return repoMgr->buildCache ( info, zypp::RepoManagerFlags::BuildIfNeeded, ProgressObserver::makeSubTask( myProgress ) );
          })
          | and_then( mtry([this, repoMgr, info = info]{
            satPool().addRepoSolv( solv_path_for_repoinfo( repoMgr->options(), info).unwrap() / "solv", info );
          }));
      })
      */
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

    expected<void> addRepo ( const RepoInfo &info ) {
      return expected<void>::success ();
    }


    /*!
     * Returns the default Pool, currently this is the only supported Pool,
     * in the future we might allow having multiple pools at the same time.
     */
    static FusionPoolRef<ContextType> defaultPool() {
      static FusionPoolRef<ContextType> me = std::make_shared<FusionPool<ContextType>>( ZYPP_PRIVATE_CONSTR_ARG_VAL );
      return me;
    }

    /*!
     * \internal Access to legacy ResPool, not exported as C API
     */
    zypp::ResPool &resPool() {
      return _pool;
    }

    /*!
     * \internal Access to legacy sat::Pool, not exported as C API
     */
    zypp::sat::Pool &satPool() {
      return _satPool;
    }

    /*!
     * \internal Access to legacy ResPoolProxy, not exported as C API
     */
    zypp::ResPoolProxy &poolProxy() {
      return _proxy;
    }

    zypp::Resolver_Ptr resolver() {
      return _resolver;
    }

  private:

    void onContextClose() {
      unloadTargetContext ();
    }

    /*!
     * Loads the given RepoInfo associated with a solvFile into the Pool.
     */
    expected<zypp::Repository> addRepository ( RepoInfo info, zypp::Pathname solvFilePath )
    {
      return expected<zypp::Repository>::error( ZYPP_EXCPT_PTR( zypp::Exception("Not implemented")) );
    }


  private:
    Ref<ContextType> _targetContext;
    zypp::ResPool _pool;
    zypp::sat::Pool _satPool;
    zypp::ResPoolProxy _proxy;
    zypp::Resolver_Ptr _resolver;
    connection _targetCloseConn;
  };

}

#endif
