/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_REPO_REFRESH_INCLUDED
#define ZYPP_NG_REPO_REFRESH_INCLUDED

#include <zypp-core/base/Gettext.h>
#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp-core/zyppng/base/Signals>
#include <zypp-core/fs/TmpPath.h>

#include <zypp/RepoManagerOptions.h>
#include <zypp/RepoManagerFlags.h>
#include <zypp/repo/PluginRepoverification.h>

#include <zypp/ng/workflows/logichelpers.h>
#include <zypp/ng/repoinfo.h>
#include <zypp/ng/resource.h>
#include <zypp/ng/context_fwd.h>

namespace zyppng {
  ZYPP_FWD_DECL_TEMPL_TYPE_WITH_REFS_ARG1 (RepoManager, ZyppContextType);
}

namespace zyppng::repo {

  ZYPP_FWD_DECL_TEMPL_TYPE_WITH_REFS_ARG1 (RefreshContext, ZyppContextType);

  using RawMetadataRefreshPolicy = zypp::RepoManagerFlags::RawMetadataRefreshPolicy;
  using RefreshCheckStatus       = zypp::RepoManagerFlags::RefreshCheckStatus;

  /*!
   *
   * Context class for all refresh workflow operations of a repo, operating
   * on a local cache and a remote repository, as defined by the \ref zypp::RepoInfo
   *
   */
  template<typename ZyppContextType>
  class RefreshContext : public Base, public MaybeAsyncMixin< std::is_same_v<ZyppContextType, AsyncContext> > {
    ZYPP_ADD_PRIVATE_CONSTR_HELPER();
    ZYPP_ENABLE_MAYBE_ASYNC_MIXIN( (std::is_same_v<ZyppContextType, AsyncContext>) );
    public:
      using ContextRefType = Ref<ZyppContextType>;
      using ContextType    = ZyppContextType;
      using ProvideType    = typename ContextType::ProvideType;
      using MediaHandle    = typename ProvideType::MediaHandle;
      using PluginRepoverification = zypp_private::repo::PluginRepoverification;
      using RepoLockRef    = zypp::Deferred;

      static expected<repo::RefreshContextRef<ZyppContextType>> create( RepoManagerRef<ContextType> repoManager, RepoInfo info )
      {
        using namespace operators;
        using CtxType    = RefreshContext<ZyppContextType>;
        using CtxRefType = RefreshContextRef<ZyppContextType>;

        return rawcache_path_for_repoinfo ( repoManager->options(), info )
        | and_then( [&]( zypp::Pathname rawCachePath ) {

          zypp::filesystem::TmpDir tmpdir( zypp::filesystem::TmpDir::makeSibling( rawCachePath, 0755 ) );
          if( tmpdir.path().empty() && geteuid() != 0 ) {
            tmpdir = zypp::filesystem::TmpDir();  // non-root user may not be able to write the cache
          }
          if( tmpdir.path().empty() ) {
            return expected<CtxRefType>::error( ZYPP_EXCPT_PTR(zypp::Exception(_("Can't create metadata cache directory."))) );
          }

          MIL << "Creating RefreshContext " << std::endl;

          return expected<CtxRefType>::success( std::make_shared<CtxType>( private_constr_t{}
                                                          , std::move(repoManager)
                                                          , std::move(info)
                                                          , std::move(rawCachePath)
                                                          , std::move(tmpdir)));
        } );
      }

      ZYPP_DECL_PRIVATE_CONSTR_ARGS(RefreshContext, RepoManagerRef<ContextType> &&repoManager, RepoInfo &&info, zypp::Pathname &&rawCachePath, zypp::filesystem::TmpDir &&tempDir )
        : _repoManager( std::move(repoManager) )
        , _repoInfo( std::move(info) )
        , _rawCachePath( std::move(rawCachePath) )
        , _tmpDir( std::move(tempDir) )
      {}

      ~RefreshContext() override
      {
        MIL << "Deleting RefreshContext" << std::endl;
      }

      /**
       * Engages the lock for the resource of the Repo to be refreshed.
       * Can be called multiple times. Returns a RAII object that
       * resets the lock once its destroyed.
       */
      MaybeAsyncRef<expected<RepoLockRef>> engageLock() {
        using namespace zyppng::operators;
        const auto lockSuccess = [this](){
          _resLockRef++;
          return expected<RepoLockRef>::success( [this](){ releaseLock(); } );
        };

        if ( _resLock ) {
          return makeReadyResult( lockSuccess() );
        }

        return this->_repoManager->zyppContext()->lockResourceWait( resources::repo::REPO_RESOURCE( _repoInfo) , 1000, ResourceLockRef::Exclusive )
            | and_then( [this]( ResourceLockRef lock ) {
              _resLock = std::move(lock);
              return expected<void>::success();
            })
            | and_then( lockSuccess );

      }


      /*!
       * Commits the internal temporary cache into the raw cache
       * of the \ref zypp::RepoInfo this context is currently operating
       * on.
       */
      void saveToRawCache()
      {
        zypp::filesystem::exchange( _tmpDir.path(), _rawCachePath );
      }

      /*!
       * The raw cache path belonging to this context's \ref zypp::RepoInfo
       */
      const zypp::Pathname &rawCachePath() const
      {
        return _rawCachePath;
      }

      /*!
       * The current temporary target dir where the refresh workflow is
       * storing the metadata during operation. Once we have a finished
       * set of metadata and cache we exchange it with the rawCache
       */
      zypp::Pathname targetDir() const
      {
        return _tmpDir.path();
      }

      /*!
       * Current zypp context we are working on, either \ref zyppng::Context
       * or \ref zyppng::SyncContext.
       */
      Ref<ZyppContextType> zyppContext() const
      {
        return _repoManager->zyppContext();
      }

      /*!
       * Current \ref zypp::RepoInfo this refresh context is operating with,
       * the workflow is free to change data in this \ref zypp::RepoInfo , so calling
       * code should take care to use it once the workflow has finished.
       */
      const RepoInfo &repoInfo () const
      {
        return _repoInfo;
      }

      RepoInfo &repoInfo ()
      {
        return _repoInfo;
      }

      /*!
       * Reference to the \ref zyppng::RepoManager that initiated the refresh
       */
      const RepoManagerRef<ZyppContextType> &repoManager() const
      {
        return _repoManager;
      }

      const zypp::RepoManagerOptions &repoManagerOptions() const
      {
        return _repoManager->options();
      }

      /*!
       * The refresh policy, defines wether a refresh is forced, or only done if needed.
       */
      RawMetadataRefreshPolicy policy() const
      {
        return _policy;
      }

      void setPolicy(RawMetadataRefreshPolicy newPolicy)
      {
        _policy = newPolicy;
      }

      /*!
       * Optional repo verification via a plugin
       */
      const std::optional<PluginRepoverification> &pluginRepoverification() const
      {
        return _pluginRepoverification;
      }

      void setPluginRepoverification( std::optional<PluginRepoverification> pluginRepoverification_r )
      { _pluginRepoverification = std::move(pluginRepoverification_r); }

      /*!
       * Disabled plugin based repo verification
       */
      void setNoPluginRepoverification()
      { setPluginRepoverification( std::nullopt ); }

      /*!
       * Updated the probed repo type in \ref zypp::RepoInfo, calling code should
       * take care to fetch the updated one at the end of the refresh workflow.
       */
      void setProbedType( zypp::repo::RepoType rType )
      {
        if ( _probedType && *_probedType == rType )
          return;

        _probedType = rType;
        _sigProbedTypeChanged.emit(rType);
      }

      const std::optional<zypp::repo::RepoType> &probedType() const
      {
        return _probedType;
      }

      SignalProxy<void(zypp::repo::RepoType)> sigProbedTypeChanged()
      {
        return _sigProbedTypeChanged;
      }

  private:

      /**
       * release the lock. Destroying the refresh context also
       * immediately releases the lock.
       */
      void releaseLock() {
        if ( _resLockRef ) {
          _resLockRef--;
          if ( _resLockRef == 0 )
            _resLock.reset();
        }
      }


      RepoManagerRef<ContextType> _repoManager;
      RepoInfo _repoInfo;
      zypp::Pathname _rawCachePath;
      zypp::filesystem::TmpDir _tmpDir;
      repo::RawMetadataRefreshPolicy _policy = RawMetadataRefreshPolicy::RefreshIfNeeded;
      std::optional<PluginRepoverification> _pluginRepoverification;  ///< \see \ref plugin-repoverification

      std::optional<ResourceLockRef> _resLock;
      uint _resLockRef = 0;

      std::optional<zypp::repo::RepoType> _probedType;
      Signal<void(zypp::repo::RepoType)> _sigProbedTypeChanged;

  };

  using SyncRefreshContext  = RefreshContext<SyncContext>;
  using AsyncRefreshContext = RefreshContext<AsyncContext>;
  ZYPP_FWD_DECL_REFS(SyncRefreshContext);
  ZYPP_FWD_DECL_REFS(AsyncRefreshContext);

}



#endif
