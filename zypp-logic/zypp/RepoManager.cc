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
#include <zypp-core/ng/pipelines/Lift>
#include <zypp/ng/progressobserveradaptor.h>
#include <zypp/ng/context.h>
#include <zypp/ng/repo/refresh.h>
#include <zypp/ng/repo/workflows/repomanagerwf.h>

#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::repomanager"

using std::endl;
using std::string;
using namespace zypp::repo;

#define OPT_PROGRESS const ProgressData::ReceiverFnc & = ProgressData::ReceiverFnc()

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  /// \class RepoManager::Impl
  /// \brief RepoManager implementation.
  ///
  ///////////////////////////////////////////////////////////////////
  struct ZYPP_LOCAL RepoManager::Impl
  {
  public:
    Impl( zyppng::ContextRef &&ctx, RepoManagerOptions &&opt) {
      _ngMgr = zyppng::RepoManager::create( std::move(ctx), std::move(opt) ).unwrap();
    }

    Impl(const Impl &) = delete;
    Impl(Impl &&) = delete;
    Impl &operator=(const Impl &) = delete;
    Impl &operator=(Impl &&) = delete;

  public:
    const zyppng::RepoManager &ngMgr() const {
      return *_ngMgr;
    }

    zyppng::RepoManager &ngMgr() {
      return *_ngMgr;
    }

  private:
    zyppng::RepoManagerRef _ngMgr;

  private:
    friend Impl * rwcowClone<Impl>( const Impl * rhs );
    /** clone for RWCOW_pointer */
    Impl * clone() const
    { return new Impl( zyppng::ContextRef(_ngMgr->zyppContext()), RepoManagerOptions(_ngMgr->options()) ); }
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
    : _pimpl( new Impl( zyppng::Context::defaultContext(), std::move(opt)) )
  {}

  RepoManager::~RepoManager()
  {}

  bool RepoManager::repoEmpty() const
  { return _pimpl->ngMgr().repoEmpty(); }

  RepoManager::RepoSizeType RepoManager::repoSize() const
  { return _pimpl->ngMgr().repoSize(); }

  RepoManager::RepoConstIterator RepoManager::repoBegin() const
  { return _pimpl->ngMgr().repoBegin(); }

  RepoManager::RepoConstIterator RepoManager::repoEnd() const
  { return _pimpl->ngMgr().repoEnd(); }

  RepoInfo RepoManager::getRepo( const std::string & alias ) const
  { return _pimpl->ngMgr().getRepo( alias ); }

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
  { return _pimpl->ngMgr().metadataStatus( info ).unwrap(); }

  RepoManager::RefreshCheckStatus RepoManager::checkIfToRefreshMetadata(const RepoInfo &info, const zypp::MirroredOrigin &origin, RawMetadataRefreshPolicy policy)
  { return _pimpl->ngMgr().checkIfToRefreshMetadata( info, origin, policy ).unwrap(); }

  RepoManager::RefreshCheckStatus RepoManager::checkIfToRefreshMetadata( const RepoInfo &info, const Url &url, RawMetadataRefreshPolicy policy )
  { return _pimpl->ngMgr().checkIfToRefreshMetadata( info, url, policy ).unwrap(); }

  Pathname RepoManager::metadataPath( const RepoInfo &info ) const
  { return _pimpl->ngMgr().metadataPath( info ).unwrap(); }

  Pathname RepoManager::packagesPath( const RepoInfo &info ) const
  { return _pimpl->ngMgr().packagesPath( info ).unwrap(); }

  void RepoManager::refreshMetadata( const RepoInfo &info, RawMetadataRefreshPolicy policy, const ProgressData::ReceiverFnc & progressrcv )
  {
    // Suppress (interactive) media::MediaChangeReport if we have fallback URLs
    zypp::media::ScopedDisableMediaChangeReport guard( info.repoOrigins().hasFallbackUrls() );
    return _pimpl->ngMgr().refreshMetadata( info, policy, nullptr ).unwrap();
  }

  void RepoManager::cleanMetadata( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->ngMgr().cleanMetadata( info, nullptr ).unwrap(); }

  void RepoManager::cleanPackages( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->ngMgr().cleanPackages( info, nullptr ).unwrap(); }

  RepoStatus RepoManager::cacheStatus( const RepoInfo &info ) const
  { return _pimpl->ngMgr().cacheStatus( info ).unwrap(); }

  void RepoManager::buildCache( const RepoInfo &info, CacheBuildPolicy policy, const ProgressData::ReceiverFnc & progressrcv )
  {
    callback::SendReport<ProgressReport> report;
    auto adapt = zyppng::ProgressObserverAdaptor( progressrcv, report );
    return _pimpl->ngMgr().buildCache( info, policy, adapt.observer() ).unwrap();
  }

  void RepoManager::cleanCache( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->ngMgr().cleanCache( info, nullptr ).unwrap(); }

  bool RepoManager::isCached( const RepoInfo &info ) const
  { return _pimpl->ngMgr().isCached( info ).unwrap(); }

  void RepoManager::loadFromCache( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->ngMgr().loadFromCache( info, nullptr ).unwrap(); }

  void RepoManager::cleanCacheDirGarbage( const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->ngMgr().cleanCacheDirGarbage( nullptr ).unwrap(); }

  repo::RepoType RepoManager::probe( const Url & url, const Pathname & path ) const
  { return _pimpl->ngMgr().probe( {url}, path ).unwrap(); }

  repo::RepoType RepoManager::probe( const Url & url ) const
  { return _pimpl->ngMgr().probe( {url} ).unwrap(); }

  void RepoManager::addRepository( const RepoInfo &info, const TriBool & forcedProbe, const ProgressData::ReceiverFnc & progressrcv )
  {
    callback::SendReport<ProgressReport> report;
    auto adapt = zyppng::ProgressObserverAdaptor( progressrcv, report );
    RepoInfo updatedRepo = _pimpl->ngMgr().addRepository( info, adapt.observer(), forcedProbe ).unwrap();

    // We should fix the API as we must inject those paths
    // into the repoinfo in order to keep it usable.
    RepoInfo & oinfo( const_cast<RepoInfo &>(info) );
    oinfo.setFilepath( updatedRepo.filepath() );
    oinfo.setMetadataPath( zyppng::rawcache_path_for_repoinfo( _pimpl->ngMgr().options(), updatedRepo ).unwrap() );
    oinfo.setPackagesPath( zyppng::packagescache_path_for_repoinfo( _pimpl->ngMgr().options(), updatedRepo ).unwrap() );
  }

  void RepoManager::addRepository( const RepoInfo &info, const ProgressData::ReceiverFnc & progressrcv )
  { addRepository( info, indeterminate, progressrcv ); }

  void RepoManager::addRepositories( const Url &url, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->ngMgr().addRepositories( url, nullptr ).unwrap(); }

  void RepoManager::removeRepository( const RepoInfo & info, const ProgressData::ReceiverFnc & progressrcv )
  {
    callback::SendReport<ProgressReport> report;
    auto adapt = zyppng::ProgressObserverAdaptor( progressrcv, report );
    return _pimpl->ngMgr().removeRepository( info, adapt.observer() ).unwrap();
  }

  void RepoManager::modifyRepository( const std::string &alias, const RepoInfo & newinfo, const ProgressData::ReceiverFnc & progressrcv )
  {
    RepoInfo updated = _pimpl->ngMgr().modifyRepository( alias, newinfo, nullptr ).unwrap();
    // We should fix the API as we must inject those paths
    // into the repoinfo in order to keep it usable.
    RepoInfo & oinfo( const_cast<RepoInfo &>(newinfo) );
    oinfo.setFilepath( updated.filepath());
    oinfo.setMetadataPath( zyppng::rawcache_path_for_repoinfo( _pimpl->ngMgr().options(), updated ).unwrap() );
    oinfo.setPackagesPath( zyppng::packagescache_path_for_repoinfo( _pimpl->ngMgr().options(), updated ).unwrap() );
  }

  RepoInfo RepoManager::getRepositoryInfo( const std::string &alias, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->ngMgr().getRepositoryInfo( alias ).unwrap(); }

  RepoInfo RepoManager::getRepositoryInfo( const Url & url, const url::ViewOption & urlview, const ProgressData::ReceiverFnc & progressrcv )
  { return _pimpl->ngMgr().getRepositoryInfo( url, urlview ).unwrap(); }

  bool RepoManager::serviceEmpty() const
  { return _pimpl->ngMgr().serviceEmpty(); }

  RepoManager::ServiceSizeType RepoManager::serviceSize() const
  { return _pimpl->ngMgr().serviceSize(); }

  RepoManager::ServiceConstIterator RepoManager::serviceBegin() const
  { return _pimpl->ngMgr().serviceBegin(); }

  RepoManager::ServiceConstIterator RepoManager::serviceEnd() const
  { return _pimpl->ngMgr().serviceEnd(); }

  ServiceInfo RepoManager::getService( const std::string & alias ) const
  { return _pimpl->ngMgr().getService( alias ); }

  bool RepoManager::hasService( const std::string & alias ) const
  { return _pimpl->ngMgr().hasService( alias ); }

  repo::ServiceType RepoManager::probeService( const Url &url ) const
  { return _pimpl->ngMgr().probeService( url ).unwrap(); }

  void RepoManager::addService( const std::string & alias, const Url& url )
  { return _pimpl->ngMgr().addService( alias, url ).unwrap(); }

  void RepoManager::addService( const ServiceInfo & service )
  { return _pimpl->ngMgr().addService( service ).unwrap(); }

  void RepoManager::removeService( const std::string & alias )
  { return _pimpl->ngMgr().removeService( alias ).unwrap(); }

  void RepoManager::removeService( const ServiceInfo & service )
  { return _pimpl->ngMgr().removeService( service ).unwrap(); }

  void RepoManager::refreshServices( const RefreshServiceOptions & options_r )
  { return _pimpl->ngMgr().refreshServices( options_r ).unwrap(); }

  void RepoManager::refreshService( const std::string & alias, const RefreshServiceOptions & options_r )
  { return _pimpl->ngMgr().refreshService( alias, options_r ).unwrap(); }

  void RepoManager::refreshService( const ServiceInfo & service, const RefreshServiceOptions & options_r )
  { return _pimpl->ngMgr().refreshService( service, options_r ).unwrap(); }

  void RepoManager::modifyService( const std::string & oldAlias, const ServiceInfo & service )
  { return _pimpl->ngMgr().modifyService( oldAlias, service ).unwrap(); }

  void RepoManager::refreshGeoIp (const RepoInfo::url_set &urls)
  { (void) _pimpl->ngMgr().refreshGeoIp( urls ); }

  ////////////////////////////////////////////////////////////////////////////

  std::ostream & operator<<( std::ostream & str, const RepoManager & obj )
  { return str << *obj._pimpl; }

  std::list<RepoInfo> readRepoFile(const Url &repo_file)
  {
    return zyppng::RepoManagerWorkflow::readRepoFile( zyppng::Context::defaultContext (), repo_file ).unwrap();
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
