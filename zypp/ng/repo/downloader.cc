/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "downloader.h"
#include <zypp/ng/Context>

#include <zypp-core/zyppng/pipelines/MTry>
#include <zypp-core/zyppng/base/Signals>
#include <zypp-core/zyppng/pipelines/Wait>
#include <zypp-core/zyppng/pipelines/Transform>
#include <zypp-core/fs/PathInfo.h>

#include <zypp/KeyRing.h>
#include <zypp-media/ng/Provide>
#include <zypp-media/ng/ProvideSpec>

#include <zypp/parser/yum/RepomdFileReader.h>

namespace zyppng::repo {


  using namespace zyppng::operators;

  template <class ContextRefType>
  DownloadContext<ContextRefType>::DownloadContext(ContextRefType zyppContext, const RepoInfo &info, const zypp::Pathname &destDir )
    : CacheProviderContext<ContextRefType>( typename CacheProviderContext<ContextRefType>::private_constr_t{}, std::move(zyppContext), destDir )
    , _repoinfo(info)
  {}

  template <class ContextRefType>
  const RepoInfo &DownloadContext<ContextRefType>::repoInfo() const { return _repoinfo; }

  template <class ContextRefType>
  const zypp::filesystem::Pathname &DownloadContext<ContextRefType>::deltaDir() const { return _deltaDir; }

  template<class ContextRefType>
  RepoInfo &DownloadContext<ContextRefType>::repoInfo() { return _repoinfo; }

  template<class ContextRefType>
  std::vector<zypp::ManagedFile> &DownloadContext<ContextRefType>::files() { return _files; }

  template<class ContextRefType>
  const std::optional<typename DownloadContext<ContextRefType>::PluginRepoverification> &DownloadContext<ContextRefType>::pluginRepoverification() const
  { return _pluginRepoverification; }

  template<class ContextRefType>
  void DownloadContext<ContextRefType>::setDeltaDir(const zypp::Pathname &newDeltaDir)
  { _deltaDir = newDeltaDir; }

  // explicitely intantiate the template types we want to work with
  template class DownloadContext<SyncContextRef>;
  template class DownloadContext<AsyncContextRef>;
}
