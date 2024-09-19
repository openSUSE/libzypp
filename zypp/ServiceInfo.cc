/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "ServiceInfo.h"
#include <zypp/ng/serviceinfo.h>
#include <zypp/zypp_detail/ZYppImpl.h>

ZYPP_BEGIN_LEGACY_API

namespace zypp {

  const ServiceInfo ServiceInfo::noService( zyppng::ServiceInfo(nullptr) );

  ServiceInfo::ServiceInfo( )
    : _pimpl( new zyppng::ServiceInfo( zypp_detail::GlobalStateHelper::context() ) )
  { }

  ServiceInfo::ServiceInfo( const zyppng::ServiceInfo &i )
    : _pimpl( std::make_unique<zyppng::ServiceInfo>(i) )
  { }

  ServiceInfo::ServiceInfo( const ServiceInfo &other )
    : _pimpl( std::make_unique<zyppng::ServiceInfo>(*other._pimpl) )
  { }

  ServiceInfo::ServiceInfo(ServiceInfo &&other)
    : _pimpl( std::move(other._pimpl) )
    , _ownsPimpl( other._ownsPimpl )
  { }

  ServiceInfo &ServiceInfo::operator=(const ServiceInfo &other)
  {
    _pimpl = std::make_unique<zyppng::ServiceInfo>(*other._pimpl);
    _ownsPimpl = true;
    return *this;
  }

  ServiceInfo &ServiceInfo::operator=(ServiceInfo &&other)
  {
    _pimpl = std::move(other._pimpl);
    _ownsPimpl = other._ownsPimpl;
    return *this;
  }

  ServiceInfo::ServiceInfo( const std::string & alias ) : _pimpl( new zyppng::ServiceInfo( zypp_detail::GlobalStateHelper::context(), alias ) ) {}

  ServiceInfo::ServiceInfo( const std::string & alias, const zypp::Url & url ) : _pimpl( new zyppng::ServiceInfo( zypp_detail::GlobalStateHelper::context(), alias, url ) ) {}

  ServiceInfo::~ServiceInfo()
  {
    if ( !_ownsPimpl ) _pimpl.release();
  }

  zypp::Url ServiceInfo::url() const
  { return _pimpl->url(); }

  zypp::Url ServiceInfo::rawUrl() const
  { return _pimpl->rawUrl(); }

  void ServiceInfo::setUrl( const zypp::Url& url )
  { _pimpl->setUrl(url); }

  zypp::repo::ServiceType ServiceInfo::type() const
  { return _pimpl->type(); }

  void ServiceInfo::setType( const zypp::repo::ServiceType & type )
  { _pimpl->setType(type); }

  void ServiceInfo::setProbedType( const zypp::repo::ServiceType &t ) const
  { _pimpl->setProbedType( t ); }

  zypp::Date::Duration ServiceInfo::ttl() const
  { return _pimpl->ttl(); }

  void ServiceInfo::setTtl( zypp::Date::Duration ttl_r )
  { return _pimpl->setTtl(ttl_r); }

  void ServiceInfo::setProbedTtl( zypp::Date::Duration ttl_r ) const
  { _pimpl->setProbedTtl(ttl_r); }

  zypp::Date ServiceInfo::lrf() const
  { return _pimpl->lrf(); }

  void ServiceInfo::setLrf( zypp::Date lrf_r )
  { _pimpl->setLrf(lrf_r); }

  bool ServiceInfo::reposToEnableEmpty() const
  { return _pimpl->reposToEnableEmpty(); }

  ServiceInfo::ReposToEnable::size_type ServiceInfo::reposToEnableSize() const
  { return _pimpl->reposToEnableSize(); }

  ServiceInfo::ReposToEnable::const_iterator ServiceInfo::reposToEnableBegin() const
  { return _pimpl->reposToEnableBegin(); }

  ServiceInfo::ReposToEnable::const_iterator ServiceInfo::reposToEnableEnd() const
  { return _pimpl->reposToEnableEnd(); }

  bool ServiceInfo::repoToEnableFind( const std::string & alias_r ) const
  { return _pimpl->repoToEnableFind(alias_r); }

  void ServiceInfo::addRepoToEnable( const std::string & alias_r )
  { _pimpl->addRepoToEnable(alias_r); }

  void ServiceInfo::delRepoToEnable( const std::string & alias_r )
  { _pimpl->delRepoToEnable(alias_r); }

  void ServiceInfo::clearReposToEnable()
  { _pimpl->clearReposToEnable(); }

  bool ServiceInfo::reposToDisableEmpty() const
  { return _pimpl->reposToDisableEmpty(); }

  ServiceInfo::ReposToDisable::size_type ServiceInfo::reposToDisableSize() const
  { return _pimpl->reposToDisableSize(); }

  ServiceInfo::ReposToDisable::const_iterator ServiceInfo::reposToDisableBegin() const
  { return _pimpl->reposToDisableBegin(); }

  ServiceInfo::ReposToDisable::const_iterator ServiceInfo::reposToDisableEnd() const
  { return _pimpl->reposToDisableEnd(); }

  bool ServiceInfo::repoToDisableFind( const std::string & alias_r ) const
  { return _pimpl->repoToDisableFind( alias_r ); }

  void ServiceInfo::addRepoToDisable( const std::string & alias_r )
  { return _pimpl->addRepoToDisable(alias_r); }

  void ServiceInfo::delRepoToDisable( const std::string & alias_r )
  { return _pimpl->delRepoToDisable(alias_r); }

  void ServiceInfo::clearReposToDisable()
  { _pimpl->clearReposToDisable(); }

  const ServiceInfo::RepoStates & ServiceInfo::repoStates() const
  { return _pimpl->repoStates(); }

  void ServiceInfo::setRepoStates( RepoStates newStates_r )
  { _pimpl->setRepoStates( std::move(newStates_r) ); }

  std::ostream & ServiceInfo::dumpAsIniOn( std::ostream & str ) const
  {
    return _pimpl->dumpAsIniOn(str);
  }

  std::ostream & ServiceInfo::dumpAsXmlOn( std::ostream & str, const std::string & content ) const
  {
    return _pimpl->dumpAsXmlOn(str, content);
  }

  zyppng::ServiceInfo &ServiceInfo::ngServiceInfo()
  {
    return *_pimpl;
  }

  const zyppng::ServiceInfo &ServiceInfo::ngServiceInfo() const
  {
    return *_pimpl;
  }

  zyppng::repo::RepoInfoBase &ServiceInfo::pimpl()
  {
    return *_pimpl;
  }

  const zyppng::repo::RepoInfoBase &ServiceInfo::pimpl() const
  {
    return *_pimpl;
  }

  std::ostream & operator<<( std::ostream& str, const ServiceInfo &obj )
  {
    return obj.dumpAsIniOn(str);
  }
}

ZYPP_END_LEGACY_API
