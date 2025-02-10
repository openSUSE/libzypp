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
#include <zypp/ng/workflows/contextfacade.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/base/Gettext.h>

namespace zyppng::repo {

  template<typename ZyppContextRefType>
  RefreshContext<ZyppContextRefType>::RefreshContext( private_constr_t, ZyppContextRefType &&zyppContext, zypp::RepoInfo &&info, zypp::Pathname &&rawCachePath, zypp::filesystem::TmpDir &&tempDir, RepoManagerRef<ContextRefType> &&repoManager )
    : _zyppContext( std::move(zyppContext) )
    , _repoManager( std::move(repoManager) )
    , _repoInfo( std::move(info) )
    , _rawCachePath( std::move(rawCachePath) )
    , _tmpDir( std::move(tempDir) )
  {
    if ( _repoManager->pluginRepoverification().checkIfNeeded() )
      _pluginRepoverification = _repoManager->pluginRepoverification();
  }

  template<typename ZyppContextRefType>
  expected<RefreshContextRef<ZyppContextRefType>> RefreshContext<ZyppContextRefType>::create( ZyppContextRefType zyppContext, zypp::RepoInfo info, RepoManagerRef<ZyppContextRefType> repoManager )
  {
    using namespace operators;
    using CtxType    = RefreshContext<ZyppContextRefType>;
    using CtxRefType = RefreshContextRef<ZyppContextRefType>;

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
                                                      , std::move(zyppContext)
                                                      , std::move(info)
                                                      , std::move(rawCachePath)
                                                      , std::move(tmpdir)
                                                      , std::move(repoManager)));
    } );
  }

  template<typename ZyppContextRefType>
  RefreshContext<ZyppContextRefType>::~RefreshContext()
  {
    MIL << "Deleting RefreshContext" << std::endl;
  }

  template<typename ZyppContextRefType>
  void RefreshContext<ZyppContextRefType>::saveToRawCache()
  {
    zypp::filesystem::exchange( _tmpDir.path(), _rawCachePath );
  }

  template<typename ZyppContextRefType>
  const zypp::Pathname &RefreshContext<ZyppContextRefType>::rawCachePath() const
  {
    return _rawCachePath;
  }

  template<typename ZyppContextRefType>
  zypp::Pathname RefreshContext<ZyppContextRefType>::targetDir() const
  {
    return _tmpDir.path();
  }

  template<typename ZyppContextRefType>
  const ZyppContextRefType &RefreshContext<ZyppContextRefType>::zyppContext() const
  {
    return _zyppContext;
  }

  template<typename ZyppContextRefType>
  const zypp::RepoInfo &RefreshContext<ZyppContextRefType>::repoInfo() const
  {
    return _repoInfo;
  }

  template<typename ZyppContextRefType>
  zypp::RepoInfo &RefreshContext<ZyppContextRefType>::repoInfo()
  {
      return _repoInfo;
  }

  template<typename ZyppContextRefType>
  const RepoManagerRef<ZyppContextRefType> &RefreshContext<ZyppContextRefType>::repoManager() const
  {
    return _repoManager;
  }

  template<typename ZyppContextRefType>
  const zypp::RepoManagerOptions &RefreshContext<ZyppContextRefType>::repoManagerOptions() const
  {
    return _repoManager->options();
  }

  template<typename ZyppContextRefType>
  repo::RawMetadataRefreshPolicy RefreshContext<ZyppContextRefType>::policy() const
  {
    return _policy;
  }

  template<typename ZyppContextRefType>
  void RefreshContext<ZyppContextRefType>::setPolicy(RawMetadataRefreshPolicy newPolicy)
  {
    _policy = newPolicy;
  }

  template<typename ZyppContextRefType>
  const std::optional<typename RefreshContext<ZyppContextRefType>::PluginRepoverification> &RefreshContext<ZyppContextRefType>::pluginRepoverification() const
  {
      return _pluginRepoverification;
  }

  template<typename ZyppContextRefType>
  void RefreshContext<ZyppContextRefType>::setProbedType(zypp::repo::RepoType rType)
  {
    if ( _probedType && *_probedType == rType )
      return;

    _probedType = rType;
    _sigProbedTypeChanged.emit(rType);
  }

  template<typename ZyppContextRefType>
  const std::optional<zypp::repo::RepoType> &RefreshContext<ZyppContextRefType>::probedType() const
  {
    return _probedType;
  }

  template<typename ZyppContextRefType>
  SignalProxy<void (zypp::repo::RepoType)> RefreshContext<ZyppContextRefType>::sigProbedTypeChanged()
  {
    return _sigProbedTypeChanged;
  }

  // explicitely intantiate the template types we want to work with
  template class RefreshContext<SyncContextRef>;
  template class RefreshContext<ContextRef>;

}
