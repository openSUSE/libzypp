/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/RepoManager.cc
 *
*/

#include "RepoManager.h"
#include <iostream>
#include <zypp-core/Digest.h>
#include <zypp-core/zyppng/pipelines/Lift>

#include <zypp/zypp_detail/ZYppImpl.h>
#include <zypp/ng/Context>
#include <zypp/ng/progressobserveradaptor.h>
#include <zypp/ng/repo/refresh.h>
#include <zypp/ng/repo/workflows/repomanagerwf.h>
#include <zypp-core/zyppng/pipelines/transform.h>

#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::repomanager"

using std::endl;
using std::string;
using namespace zypp::repo;

#define OPT_PROGRESS const ProgressData::ReceiverFnc & = ProgressData::ReceiverFnc()

///////////////////////////////////////////////////////////////////
namespace zypp
{

  ZYPP_BEGIN_LEGACY_API

  ///////////////////////////////////////////////////////////////////
  /// \class RepoManager::Impl
  /// \brief RepoManager implementation.
  ///
  ///////////////////////////////////////////////////////////////////
  struct ZYPP_LOCAL RepoManager::Impl
  {
  public:
    Impl( zyppng::SyncContextRef &&ctx, RepoManagerOptions &&opt) {
      _ngMgr = zyppng::SyncRepoManager::create( std::move(ctx), std::move(opt) );
      _ngMgr->initialize().unwrap();
      sync();
    }

    Impl(const Impl &) = delete;
    Impl(Impl &&) = delete;
    Impl &operator=(const Impl &) = delete;
    Impl &operator=(Impl &&) = delete;

  public:
    const zyppng::SyncRepoManager &ngMgr() const {
      return *_ngMgr;
    }

    zyppng::SyncRepoManager &ngMgr() {
      return *_ngMgr;
    }

    zyppng::SyncRepoManagerRef ngMgrRef() {
      return _ngMgr;
    }

    void syncRepos() {
      auto &ngRepos = _ngMgr->reposManip();
      _repos.clear();

      // add repos we do not know yet
      for ( const auto &r : ngRepos ) {
        // COW semantics apply
        zypp::RepoInfo tmp(r.second);
        if ( _repos.count(tmp) == 0 )
          _repos.insert( std::move(tmp) );
      }
    }

    void syncServices() {
      auto &ngServices = _ngMgr->servicesManip();
      _services.clear();

      // add repos we do not know yet
      for ( const auto &s : ngServices ) {
        // COW semantics apply
        zypp::ServiceInfo tmp = s.second;
        if ( _services.count(tmp) == 0 )
          _services.insert( std::move(tmp) );
      }
    }

    void sync() {
      // we need to keep source level compatibility, and because we return direct iterators
      // into our service and repo sets we need to provide real sets of the legacy RepoInfo and ServiceInfo
      // using sync'ed sets is the only way I could think of for now
      syncRepos();
      syncServices();
    }

    ServiceSet _services;
    RepoSet _repos;

  private:
    zyppng::SyncRepoManagerRef _ngMgr;

  private:
    friend Impl * rwcowClone<Impl>( const Impl * rhs );
    /** clone for RWCOW_pointer */
    Impl * clone() const
    { return new Impl( zyppng::SyncContextRef(_ngMgr->zyppContext()), RepoManagerOptions(_ngMgr->options()) ); }
  };
  ///////////////////////////////////////////////////////////////////

  /** \relates RepoManager::Impl Stream output */
  inline std::ostream & operator<<( std::ostream & str, const RepoManager::Impl & obj )
  { return str << "RepoManager::Impl"; }

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : RepoManager
  //
  ///////////////////////////////////////////////////////////////////

  RepoManager::RepoManager( RepoManagerOptions opt )
    : _pimpl( new Impl( zypp_detail::GlobalStateHelper::context(), std::move(opt)) )
  {}

  RepoManager::~RepoManager()
  {}

  bool RepoManager::repoEmpty() const
  { return _pimpl->ngMgr().repoEmpty(); }

  RepoManager::RepoSizeType RepoManager::repoSize() const
  { return _pimpl->ngMgr().repoSize(); }

  RepoManager::RepoConstIterator RepoManager::repoBegin() const
  { return _pimpl->_repos.begin(); }

  RepoManager::RepoConstIterator RepoManager::repoEnd() const
  { return _pimpl->_repos.end(); }

  RepoInfo RepoManager::getRepo( const std::string & alias ) const
  {
    const auto &optRepo = _pimpl->ngMgr().getRepo( alias );
    if ( !optRepo )
      return RepoInfo();
    return RepoInfo(*optRepo);
  }

  bool RepoManager::hasRepo( const std::string & alias ) const
  { return _pimpl->ngMgr().hasRepo( alias ); }

  std::string RepoManager::makeStupidAlias( const Url & url_r )
  {
    std::string ret( url_r.getScheme() );
    if ( ret.empty() )
      ret = "repo-";
    else
      ret += "-";

    std::string host( url_r.getHost() );
    if ( ! host.empty() )
    {
      ret += host;
      ret += "-";
    }

    static Date::ValueType serial = Date::now();
    ret += Digest::digest( Digest::sha1(), str::hexstring( ++serial ) +url_r.asCompleteString() ).substr(0,8);
    return ret;
  }

  RepoStatus RepoManager::metadataStatus( const RepoInfo & info ) const
  { return _pimpl->ngMgr().metadataStatus( info.ngRepoInfo() ).unwrap(); }

  RepoManager::RefreshCheckStatus RepoManager::checkIfToRefreshMetadata( const RepoInfo &info, const Url &url, RawMetadataRefreshPolicy policy )
  {
    auto res = _pimpl->ngMgr().checkIfToRefreshMetadata( const_cast<RepoInfo &>(info).ngRepoInfo() , url, policy ).unwrap();
    const_cast<RepoInfo &>(info).ngRepoInfo() = res.first;
    _pimpl->syncRepos();
    return res.second;
  }

  Pathname RepoManager::metadataPath( const RepoInfo &info ) const
  { return _pimpl->ngMgr().metadataPath( info.ngRepoInfo() ).unwrap(); }

  Pathname RepoManager::packagesPath( const RepoInfo &info ) const
  { return _pimpl->ngMgr().packagesPath( info.ngRepoInfo() ).unwrap(); }

  void RepoManager::refreshMetadata( const RepoInfo &info, RawMetadataRefreshPolicy policy, const ProgressData::ReceiverFnc & progressrcv )
  {
    // Suppress (interactive) media::MediaChangeReport if we in have multiple basurls (>1)
    zypp::media::ScopedDisableMediaChangeReport guard( info.baseUrlsSize() > 1 );
    const_cast<RepoInfo &>(info).ngRepoInfo() = _pimpl->ngMgr().refreshMetadata( info.ngRepoInfo(), policy, nullptr ).unwrap();
    _pimpl->syncRepos();
  }

  void RepoManager::cleanMetadata( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->ngMgr().cleanMetadata( info.ngRepoInfo(), nullptr ).unwrap(); }

  void RepoManager::cleanPackages( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->ngMgr().cleanPackages( info.ngRepoInfo(), nullptr ).unwrap(); }

  RepoStatus RepoManager::cacheStatus( const RepoInfo &info ) const
  { return _pimpl->ngMgr().cacheStatus( info.ngRepoInfo() ).unwrap(); }

  void RepoManager::buildCache( const RepoInfo &info, CacheBuildPolicy policy, const ProgressData::ReceiverFnc & progressrcv )
  {
    callback::SendReport<ProgressReport> report;
    auto adapt = zyppng::ProgressObserverAdaptor( progressrcv, report );
    const_cast<RepoInfo &>(info).ngRepoInfo() = _pimpl->ngMgr().buildCache( const_cast<RepoInfo &>(info).ngRepoInfo(), policy, adapt.observer() ).unwrap();
    _pimpl->syncRepos();
  }

  void RepoManager::cleanCache( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->ngMgr().cleanCache( info.ngRepoInfo(), nullptr ).unwrap(); }

  bool RepoManager::isCached( const RepoInfo &info ) const
  { return _pimpl->ngMgr().isCached( info.ngRepoInfo() ).unwrap(); }

  void RepoManager::loadFromCache( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { return zypp_detail::GlobalStateHelper::pool ()->loadFromCache ( info.ngRepoInfo() , nullptr ).unwrap(); }

  void RepoManager::cleanCacheDirGarbage( const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->ngMgr().cleanCacheDirGarbage( nullptr ).unwrap(); }

  repo::RepoType RepoManager::probe( const Url & url, const Pathname & path ) const
  { return _pimpl->ngMgr().probe( url, path ).unwrap(); }

  repo::RepoType RepoManager::probe( const Url & url ) const
  { return _pimpl->ngMgr().probe( url ).unwrap(); }

  void RepoManager::addRepository( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  {
    callback::SendReport<ProgressReport> report;
    auto adapt = zyppng::ProgressObserverAdaptor( progressrcv, report );
    const_cast<RepoInfo &>(info).ngRepoInfo() = _pimpl->ngMgr().addRepository( info.ngRepoInfo (), adapt.observer() ).unwrap();
    _pimpl->syncRepos();
  }

  void RepoManager::addRepositories( const Url &url, const ProgressData::ReceiverFnc & progressrcv )
  {
    _pimpl->ngMgr().addRepositories( url, nullptr ).unwrap();
    _pimpl->syncRepos();
  }

  void RepoManager::removeRepository( const RepoInfo & info, const ProgressData::ReceiverFnc & progressrcv )
  {
    callback::SendReport<ProgressReport> report;
    auto adapt = zyppng::ProgressObserverAdaptor( progressrcv, report );
    _pimpl->ngMgr().removeRepository( const_cast<RepoInfo &>(info).ngRepoInfo(), adapt.observer() ).unwrap();
    _pimpl->syncRepos();
  }

  void RepoManager::modifyRepository( const std::string &alias, const RepoInfo & newinfo, const ProgressData::ReceiverFnc & progressrcv )
  {
    // We should fix the API as we must inject those paths
    // into the repoinfo in order to keep it usable.
    const_cast<RepoInfo &>(newinfo).ngRepoInfo() = _pimpl->ngMgr().modifyRepository( alias, newinfo.ngRepoInfo(), nullptr ).unwrap();
    _pimpl->syncRepos();
  }

  RepoInfo RepoManager::getRepositoryInfo( const std::string &alias, const ProgressData::ReceiverFnc & progressrcv )
  {
    return RepoInfo(_pimpl->ngMgr().getRepositoryInfo( alias ).unwrap());
  }

  RepoInfo RepoManager::getRepositoryInfo( const Url & url, const url::ViewOption & urlview, const ProgressData::ReceiverFnc & progressrcv )
  { return RepoInfo(_pimpl->ngMgr().getRepositoryInfo( url, urlview ).unwrap()); }

  bool RepoManager::serviceEmpty() const
  { return _pimpl->ngMgr().serviceEmpty(); }

  RepoManager::ServiceSizeType RepoManager::serviceSize() const
  { return _pimpl->ngMgr().serviceSize(); }

  RepoManager::ServiceConstIterator RepoManager::serviceBegin() const
  { return _pimpl->_services.begin(); }

  RepoManager::ServiceConstIterator RepoManager::serviceEnd() const
  { return _pimpl->_services.end(); }

  ServiceInfo RepoManager::getService( const std::string & alias ) const
  {
    const auto &optService = _pimpl->ngMgr().getService( alias );
    if ( !optService ) return ServiceInfo();
    return ServiceInfo(*optService);
  }

  bool RepoManager::hasService( const std::string & alias ) const
  { return _pimpl->ngMgr().hasService( alias ); }

  repo::ServiceType RepoManager::probeService( const Url &url ) const
  { return _pimpl->ngMgr().probeService( url ).unwrap(); }

  void RepoManager::addService( const std::string & alias, const Url& url )
  {
    _pimpl->ngMgr().addService( alias, url ).unwrap();
    _pimpl->syncServices();
  }

  void RepoManager::addService( const ServiceInfo & service )
  {
    _pimpl->ngMgr().addService( service.ngServiceInfo() ).unwrap();
    _pimpl->syncServices();
  }

  void RepoManager::removeService( const std::string & alias )
  {
    _pimpl->ngMgr().removeService( alias ).unwrap();
    _pimpl->syncServices();
  }

  void RepoManager::removeService( const ServiceInfo & service )
  {
    _pimpl->ngMgr().removeService( service.ngServiceInfo() ).unwrap();
    _pimpl->syncServices();
  }

  void RepoManager::refreshServices( const RefreshServiceOptions & options_r )
  {
    _pimpl->ngMgr().refreshServices( options_r ).unwrap();
    _pimpl->sync();
  }

  void RepoManager::refreshService( const std::string & alias, const RefreshServiceOptions & options_r )
  {
    _pimpl->ngMgr().refreshService( alias, options_r ).unwrap();
    _pimpl->sync();
  }

  void RepoManager::refreshService( const ServiceInfo & service, const RefreshServiceOptions & options_r )
  {
    _pimpl->ngMgr().refreshService( service.ngServiceInfo(), options_r ).unwrap();
    _pimpl->sync();
  }

  void RepoManager::modifyService( const std::string & oldAlias, const ServiceInfo & service )
  {
    _pimpl->ngMgr().modifyService( oldAlias, service.ngServiceInfo() ).unwrap();
    _pimpl->syncServices();
  }

  void RepoManager::refreshGeoIp (const RepoInfo::url_set &urls)
  {
    (void) zyppng::RepoManagerWorkflow::refreshGeoIPData( _pimpl->ngMgr().zyppContext(), urls);
  }

  ////////////////////////////////////////////////////////////////////////////

  std::ostream & operator<<( std::ostream & str, const RepoManager & obj )
  { return str << *obj._pimpl; }

  std::list<RepoInfo> readRepoFile(const Url &repo_file)
  {
    std::list<RepoInfo> rIs;

    auto ngList = zyppng::RepoManagerWorkflow::readRepoFile( zypp_detail::GlobalStateHelper::context(), repo_file ).unwrap();
    for ( const auto &ngRi : ngList )
      rIs.push_back( zypp::RepoInfo(ngRi) );

    return rIs;
  }

  ZYPP_END_LEGACY_API

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
