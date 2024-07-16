/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_REPO_DOWNLOADER_INCLUDED
#define ZYPP_NG_REPO_DOWNLOADER_INCLUDED

#include <zypp-core/zyppng/base/Base>
#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp-core/Pathname.h>
#include <zypp-core/OnMediaLocation>
#include <zypp-core/ByteCount.h>
#include <zypp-core/ManagedFile.h>
#include <zypp-media/ng/ProvideRes>
#include <zypp/repo/PluginRepoverification.h>
#include <zypp/RepoStatus.h>
#include <zypp/RepoInfo.h>

#include <zypp/ng/workflows/downloadwf.h>
#include <zypp/ng/context_fwd.h>

#include <optional>

namespace zyppng::repo {
  template <class ContextRefType>
  class DownloadContext : public CacheProviderContext<ContextRefType>
  {
  public:

    using PluginRepoverification = zypp_private::repo::PluginRepoverification;
    using ContextType   = typename ContextRefType::element_type;
    using ProvideType   = typename ContextType::ProvideType;
    using MediaHandle   = typename ProvideType::MediaHandle;

    DownloadContext( ContextRefType zyppContext, const zypp::RepoInfo &info, const zypp::Pathname &destDir );

    const zypp::RepoInfo &repoInfo () const;
    const zypp::Pathname &deltaDir () const;

    zypp::RepoInfo &repoInfo();
    std::vector<zypp::ManagedFile> &files();

    const std::optional<PluginRepoverification> &pluginRepoverification() const;

    void setPluginRepoverification( std::optional<PluginRepoverification> pluginRepoverification_r )
    { _pluginRepoverification = std::move(pluginRepoverification_r); }

    void setNoPluginRepoverification()
    { setPluginRepoverification( std::nullopt ); }

    void setDeltaDir(const zypp::Pathname &newDeltaDir);

  private:
    zypp::RepoInfo _repoinfo;
    zypp::Pathname _deltaDir;
    std::vector<zypp::ManagedFile> _files; ///< Files downloaded
    std::optional<PluginRepoverification> _pluginRepoverification;  ///< \see \ref plugin-repoverification
  };

  using SyncDownloadContext  = DownloadContext<SyncContextRef>;
  using AsyncDownloadContext = DownloadContext<AsyncContextRef>;
  ZYPP_FWD_DECL_REFS(SyncDownloadContext);
  ZYPP_FWD_DECL_REFS(AsyncDownloadContext);
}
#endif
