/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "refresh.h"
#include <zypp-media/ng/providespec.h>
#include <zypp/ng/Context>
#include <zypp/ng/repomanager.h>

#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/base/Gettext.h>

namespace zyppng::repo {

  template<typename ZyppContextType>
  RefreshContext<ZyppContextType>::RefreshContext( private_constr_t, RepoManagerRef<ContextType> &&repoManager, RepoInfo &&info, zypp::Pathname &&rawCachePath, zypp::filesystem::TmpDir &&tempDir )
    : _repoManager( std::move(repoManager) )
    , _repoInfo( std::move(info) )
    , _rawCachePath( std::move(rawCachePath) )
    , _tmpDir( std::move(tempDir) )
  {}

  template<typename ZyppContextType>
  expected<RefreshContextRef<ZyppContextType>> RefreshContext<ZyppContextType>::create( RepoManagerRef<ContextType> repoManager, RepoInfo info )
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

  template<typename ZyppContextType>
  RefreshContext<ZyppContextType>::~RefreshContext()
  {
    MIL << "Deleting RefreshContext" << std::endl;
  }

  template<typename ZyppContextType>
  void RefreshContext<ZyppContextType>::saveToRawCache()
  {
    zypp::filesystem::exchange( _tmpDir.path(), _rawCachePath );
  }

  template<typename ZyppContextType>
  const zypp::Pathname &RefreshContext<ZyppContextType>::rawCachePath() const
  {
    return _rawCachePath;
  }

  template<typename ZyppContextType>
  zypp::Pathname RefreshContext<ZyppContextType>::targetDir() const
  {
    return _tmpDir.path();
  }

  template<typename ZyppContextType>
  Ref<ZyppContextType> RefreshContext<ZyppContextType>::zyppContext() const
  {
    return _repoManager->zyppContext();
  }

  template<typename ZyppContextType>
  const RepoInfo &RefreshContext<ZyppContextType>::repoInfo() const
  {
    return _repoInfo;
  }

  template<typename ZyppContextType>
  RepoInfo &RefreshContext<ZyppContextType>::repoInfo()
  {
      return _repoInfo;
  }

  template<typename ZyppContextType>
  const RepoManagerRef<ZyppContextType> &RefreshContext<ZyppContextType>::repoManager() const
  {
    return _repoManager;
  }

  template<typename ZyppContextType>
  const zypp::RepoManagerOptions &RefreshContext<ZyppContextType>::repoManagerOptions() const
  {
    return _repoManager->options();
  }

  template<typename ZyppContextType>
  repo::RawMetadataRefreshPolicy RefreshContext<ZyppContextType>::policy() const
  {
    return _policy;
  }

  template<typename ZyppContextType>
  void RefreshContext<ZyppContextType>::setPolicy(RawMetadataRefreshPolicy newPolicy)
  {
    _policy = newPolicy;
  }

  template<typename ZyppContextType>
  const std::optional<typename RefreshContext<ZyppContextType>::PluginRepoverification> &RefreshContext<ZyppContextType>::pluginRepoverification() const
  {
      return _pluginRepoverification;
  }

  template<typename ZyppContextType>
  void RefreshContext<ZyppContextType>::setProbedType(zypp::repo::RepoType rType)
  {
    if ( _probedType && *_probedType == rType )
      return;

    _probedType = rType;
    _sigProbedTypeChanged.emit(rType);
  }

  template<typename ZyppContextType>
  const std::optional<zypp::repo::RepoType> &RefreshContext<ZyppContextType>::probedType() const
  {
    return _probedType;
  }

  template<typename ZyppContextType>
  SignalProxy<void (zypp::repo::RepoType)> RefreshContext<ZyppContextType>::sigProbedTypeChanged()
  {
    return _sigProbedTypeChanged;
  }

  // explicitely intantiate the template types we want to work with
  template class RefreshContext<SyncContext>;
  template class RefreshContext<AsyncContext>;

}
