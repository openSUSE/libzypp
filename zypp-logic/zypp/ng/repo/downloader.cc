/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "downloader.h"
#include <zypp-core/ng/pipelines/MTry>
#include <zypp-core/ng/base/Signals>
#include <zypp-core/ng/pipelines/Transform>
#include <zypp-core/fs/PathInfo.h>

#include <zypp/KeyRing.h>
#include <zypp/ng/Context>
#include <zypp/ng/media/provide.h>

#include <zypp/parser/yum/RepomdFileReader.h>

namespace zyppng::repo {


  using namespace zyppng::operators;


  DownloadContext::DownloadContext(ContextRef zyppContext, const zypp::RepoInfo &info, const zypp::Pathname &destDir )
    : CacheProviderContext( typename CacheProviderContext::private_constr_t{}, std::move(zyppContext), destDir )
    , _repoinfo(info)
  {}

  const zypp::RepoInfo &DownloadContext::repoInfo() const { return _repoinfo; }

  const zypp::filesystem::Pathname &DownloadContext::deltaDir() const { return _deltaDir; }

  zypp::RepoInfo &DownloadContext::repoInfo() { return _repoinfo; }

  std::vector<zypp::ManagedFile> &DownloadContext::files() { return _files; }

  const std::optional<typename DownloadContext::PluginRepoverification> &DownloadContext::pluginRepoverification() const
  { return _pluginRepoverification; }

  void DownloadContext::setDeltaDir(const zypp::Pathname &newDeltaDir)
  { _deltaDir = newDeltaDir; }
}
