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

#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp-core/zyppng/base/Signals>
#include <zypp-core/fs/TmpPath.h>

#include <zypp/RepoInfo.h>
#include <zypp/RepoManagerOptions.h>
#include <zypp/RepoManagerFlags.h>
#include <zypp/ng/repomanager.h>
#include <zypp/repo/PluginRepoverification.h>
#include <zypp/ng/workflows/logichelpers.h>

namespace zyppng {
  ZYPP_FWD_DECL_TYPE_WITH_REFS( Context );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( SyncContext );
}

namespace zyppng::repo {

  ZYPP_FWD_DECL_TEMPL_TYPE_WITH_REFS_ARG1 (RefreshContext, ZyppContextRefType);

  using RawMetadataRefreshPolicy = zypp::RepoManagerFlags::RawMetadataRefreshPolicy;
  using RefreshCheckStatus       = zypp::RepoManagerFlags::RefreshCheckStatus;

  /*!
   *
   * Context class for all refresh workflow operations of a repo, operating
   * on a local cache and a remote repository, as defined by the \ref zypp::RepoInfo
   *
   */
  template<typename ZyppContextRefType>
  class RefreshContext : public Base, public MaybeAsyncMixin< std::is_same_v<ZyppContextRefType, ContextRef> > {
    ZYPP_ADD_PRIVATE_CONSTR_HELPER();
    ZYPP_ENABLE_MAYBE_ASYNC_MIXIN( (std::is_same_v<ZyppContextRefType, ContextRef>) );
    public:
      using ContextRefType = ZyppContextRefType;
      using ContextType    = typename ZyppContextRefType::element_type;
      using ProvideType    = typename ContextType::ProvideType;
      using MediaHandle    = typename ProvideType::MediaHandle;
      using PluginRepoverification = zypp_private::repo::PluginRepoverification;

      static expected<repo::RefreshContextRef<ZyppContextRefType>> create( ZyppContextRefType zyppContext, zypp::RepoInfo info, RepoManagerRef<ContextRefType> repoManager );
      ZYPP_DECL_PRIVATE_CONSTR_ARGS(RefreshContext, ZyppContextRefType &&zyppContext, zypp::RepoInfo &&info, zypp::Pathname &&rawCachePath, zypp::filesystem::TmpDir &&tempDir, RepoManagerRef<ContextRefType> &&repoManager );

      ~RefreshContext() override;

      /*!
       * Commits the internal temporary cache into the raw cache
       * of the \ref zypp::RepoInfo this context is currently operating
       * on.
       */
      void saveToRawCache();

      /*!
       * The raw cache path belonging to this context's \ref zypp::RepoInfo
       */
      const zypp::Pathname &rawCachePath() const;

      /*!
       * The current temporary target dir where the refresh workflow is
       * storing the metadata during operation. Once we have a finished
       * set of metadata and cache we exchange it with the rawCache
       */
      zypp::Pathname targetDir() const;

      /*!
       * Current zypp context we are working on, either \ref zyppng::Context
       * or \ref zyppng::SyncContext.
       */
      const ZyppContextRefType &zyppContext () const;

      /*!
       * Current \ref zypp::RepoInfo this refresh context is operating with,
       * the workflow is free to change data in this \ref zypp::RepoInfo , so calling
       * code should take care to use it once the workflow has finished.
       */
      const zypp::RepoInfo &repoInfo () const;
      zypp::RepoInfo &repoInfo ();

      /*!
       * Reference to the \ref zyppng::RepoManager that initiated the refresh
       */
      const RepoManagerRef<ZyppContextRefType> &repoManager() const;
      const zypp::RepoManagerOptions &repoManagerOptions() const;

      /*!
       * The refresh policy, defines wether a refresh is forced, or only done if needed.
       */
      RawMetadataRefreshPolicy policy() const;
      void setPolicy(RawMetadataRefreshPolicy newPolicy);

      /*!
       * Optional repo verification via a plugin
       */
      const std::optional<PluginRepoverification> &pluginRepoverification() const;

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
      void setProbedType( zypp::repo::RepoType rType );
      const std::optional<zypp::repo::RepoType> &probedType() const;
      SignalProxy<void(zypp::repo::RepoType)> sigProbedTypeChanged();

  private:
      ZyppContextRefType _zyppContext;
      RepoManagerRef<ContextRefType> _repoManager;
      zypp::RepoInfo _repoInfo;
      zypp::Pathname _rawCachePath;
      zypp::filesystem::TmpDir _tmpDir;
      repo::RawMetadataRefreshPolicy _policy = RawMetadataRefreshPolicy::RefreshIfNeeded;
      std::optional<PluginRepoverification> _pluginRepoverification;  ///< \see \ref plugin-repoverification

      std::optional<zypp::repo::RepoType> _probedType;
      Signal<void(zypp::repo::RepoType)> _sigProbedTypeChanged;

  };

  using SyncRefreshContext  = RefreshContext<SyncContextRef>;
  using AsyncRefreshContext = RefreshContext<ContextRef>;
  ZYPP_FWD_DECL_REFS(SyncRefreshContext);
  ZYPP_FWD_DECL_REFS(AsyncRefreshContext);

}



#endif
