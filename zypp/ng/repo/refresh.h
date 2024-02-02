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
#include <zypp/repo/PluginRepoverification.h>

namespace zyppng {
  ZYPP_FWD_DECL_TYPE_WITH_REFS( Context );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( SyncContext );
}

namespace zyppng::repo {

  ZYPP_FWD_DECL_TEMPL_TYPE_WITH_REFS_ARG1 (RefreshContext, ZyppContextRefType);

  enum RawMetadataRefreshPolicy
  {
    RefreshIfNeeded,
    RefreshForced,
    RefreshIfNeededIgnoreDelay
  };

  /**
   * Possibly return state of checkIfRefreshMEtadata function
   */
  enum RefreshCheckStatus {
    REFRESH_NEEDED,     /**< refresh is needed */
    REPO_UP_TO_DATE,    /**< repository not changed */
    REPO_CHECK_DELAYED  /**< refresh is delayed due to settings */
  };

  /*!
   *
   * Context class for all refresh workflow operations of a repo, operating
   * on a local cache and a remote repository, as defined by the \ref zypp::RepoInfo
   *
   */
  template<typename ZyppContextRefType>
  class RefreshContext : public Base {
    ZYPP_ADD_PRIVATE_CONSTR_HELPER();
    public:
      using ContextRefType = ZyppContextRefType;
      using ContextType    = typename ZyppContextRefType::element_type;
      using ProvideType    = typename ContextType::ProvideType;
      using MediaHandle    = typename ProvideType::MediaHandle;
      using PluginRepoverification = zypp_private::repo::PluginRepoverification;

      static expected<repo::RefreshContextRef<ZyppContextRefType>> create( ZyppContextRefType zyppContext, zypp::RepoInfo info, zypp::RepoManagerOptions opts );
      ZYPP_DECL_PRIVATE_CONSTR_ARGS(RefreshContext, ZyppContextRefType &&zyppContext, zypp::RepoInfo &&info, zypp::Pathname &&rawCachePath, zypp::filesystem::TmpDir &&tempDir, zypp::RepoManagerOptions &&opts );

      ~RefreshContext() override;

      void saveToRawCache();

      const zypp::Pathname &rawCachePath() const;
      zypp::Pathname targetDir() const;
      const ZyppContextRefType &zyppContext () const;
      const zypp::RepoInfo &repoInfo () const;
      zypp::RepoInfo &repoInfo ();
      const zypp::RepoManagerOptions &repoManagerOptions() const;

      repo::RawMetadataRefreshPolicy policy() const;
      void setPolicy(repo::RawMetadataRefreshPolicy newPolicy);

      const std::optional<PluginRepoverification> &pluginRepoverification() const;

      void setPluginRepoverification( std::optional<PluginRepoverification> pluginRepoverification_r )
      { _pluginRepoverification = std::move(pluginRepoverification_r); }

      void setNoPluginRepoverification()
      { setPluginRepoverification( std::nullopt ); }

      void setProbedType( zypp::repo::RepoType rType );
      const std::optional<zypp::repo::RepoType> &probedType() const;
      SignalProxy<void(zypp::repo::RepoType)> sigProbedTypeChanged();

  private:
      ZyppContextRefType _zyppContext;
      zypp::RepoInfo _repoInfo;
      zypp::Pathname _rawCachePath;
      zypp::filesystem::TmpDir _tmpDir;
      repo::RawMetadataRefreshPolicy _policy = repo::RefreshIfNeeded;
      std::optional<PluginRepoverification> _pluginRepoverification;  ///< \see \ref plugin-repoverification
      zypp::RepoManagerOptions _repoManagerOptions;

      std::optional<zypp::repo::RepoType> _probedType;
      Signal<void(zypp::repo::RepoType)> _sigProbedTypeChanged;

  };

  using SyncRefreshContext  = RefreshContext<SyncContextRef>;
  using AsyncRefreshContext = RefreshContext<ContextRef>;
  ZYPP_FWD_DECL_REFS(SyncRefreshContext);
  ZYPP_FWD_DECL_REFS(AsyncRefreshContext);

}



#endif
