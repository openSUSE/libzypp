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
#include <zypp/repo/RepoMirrorList.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/base/Gettext.h>

namespace zyppng::repo {


  RefreshContext::RefreshContext( private_constr_t, ContextRef &&zyppContext, zypp::RepoInfo &&info, zypp::Pathname &&rawCachePath, zypp::filesystem::TmpDir &&tempDir, RepoManagerRef &&repoManager )
    : _zyppContext( std::move(zyppContext) )
    , _repoManager( std::move(repoManager) )
    , _repoInfo( std::move(info) )
    , _rawCachePath( std::move(rawCachePath) )
    , _tmpDir( std::move(tempDir) )
  {
    if ( _repoManager->pluginRepoverification().checkIfNeeded() )
      _pluginRepoverification = _repoManager->pluginRepoverification();
  }


  expected<RefreshContextRef> RefreshContext::create( ContextRef zyppContext, zypp::RepoInfo info, RepoManagerRef repoManager )
  {
    using namespace operators;
    using CtxType    = RefreshContext;
    using CtxRefType = RefreshContextRef;

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


  RefreshContext::~RefreshContext()
  {
    MIL << "Deleting RefreshContext" << std::endl;
  }


  void RefreshContext::saveToRawCache()
  {
    // RepoMirrorList may have updated the mirrorlist file in the old
    // metadata dir. We transfer it to the new dir.
    zypp::Pathname oldCache = _rawCachePath / zypp::repo::RepoMirrorList::cacheFileName();
    zypp::Pathname newCache = _tmpDir.path() / zypp::repo::RepoMirrorList::cacheFileName();
    if ( zypp::PathInfo(oldCache).isExist() && not zypp::PathInfo(newCache).isExist() ) {
      for ( const auto & f : { zypp::repo::RepoMirrorList::cacheFileName(), zypp::repo::RepoMirrorList::cookieFileName() } ) {
        zypp::filesystem::hardlinkCopy( (_rawCachePath / f), (_tmpDir.path() / f) );
      }
    }
    zypp::filesystem::exchange( _tmpDir.path(), _rawCachePath );
  }


  const zypp::Pathname &RefreshContext::rawCachePath() const
  {
    return _rawCachePath;
  }


  zypp::Pathname RefreshContext::targetDir() const
  {
    return _tmpDir.path();
  }


  const ContextRef &RefreshContext::zyppContext() const
  {
    return _zyppContext;
  }


  const zypp::RepoInfo &RefreshContext::repoInfo() const
  {
    return _repoInfo;
  }


  zypp::RepoInfo &RefreshContext::repoInfo()
  {
      return _repoInfo;
  }


  const RepoManagerRef &RefreshContext::repoManager() const
  {
    return _repoManager;
  }


  const zypp::RepoManagerOptions &RefreshContext::repoManagerOptions() const
  {
    return _repoManager->options();
  }


  repo::RawMetadataRefreshPolicy RefreshContext::policy() const
  {
    return _policy;
  }


  void RefreshContext::setPolicy(RawMetadataRefreshPolicy newPolicy)
  {
    _policy = newPolicy;
  }


  const std::optional<typename RefreshContext::PluginRepoverification> &RefreshContext::pluginRepoverification() const
  {
      return _pluginRepoverification;
  }


  void RefreshContext::setProbedType(zypp::repo::RepoType rType)
  {
    if ( _probedType && *_probedType == rType )
      return;

    _probedType = rType;
    _sigProbedTypeChanged.emit(rType);
  }


  const std::optional<zypp::repo::RepoType> &RefreshContext::probedType() const
  {
    return _probedType;
  }


  SignalProxy<void (zypp::repo::RepoType)> RefreshContext::sigProbedTypeChanged()
  {
    return _sigProbedTypeChanged;
  }

}
