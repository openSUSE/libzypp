/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ng/ServiceInfo.h
 *
 */
#ifndef ZYPP_NG_SERVICE_INFO_SHARED_DATA_H_INCLUDED
#define ZYPP_NG_SERVICE_INFO_SHARED_DATA_H_INCLUDED

#include <zypp-core/base/defaultintegral.h>
#include <zypp/ng/repo/repoinfobaseshareddata.h>
#include <zypp/ng/serviceinfo.h>

namespace zyppng
{

  struct ServiceInfoSharedData : public repo::RepoInfoBaseSharedData
  {
    using ReposToEnable = ServiceInfo::ReposToEnable;
    using ReposToDisable = ServiceInfo::ReposToDisable;

  public:
    RepoVariablesReplacedUrl _url;
    zypp::repo::ServiceType _type;
    ReposToEnable _reposToEnable;
    ReposToDisable _reposToDisable;
    ServiceInfo::RepoStates _repoStates;
    zypp::DefaultIntegral<zypp::Date::Duration,0> _ttl;
    zypp::Date _lrf;

  public:
    ServiceInfoSharedData( ContextBaseRef &&context );
    ServiceInfoSharedData( ContextBaseRef &&context, const std::string &alias );
    ServiceInfoSharedData( ContextBaseRef &&context, const std::string &alias, const zypp::Url &url_r );

    ServiceInfoSharedData( ServiceInfoSharedData && ) = delete;
    ServiceInfoSharedData &operator=( const ServiceInfoSharedData & ) = delete;
    ServiceInfoSharedData &operator=( ServiceInfoSharedData && ) = delete;

    ~ServiceInfoSharedData() override
    {}

    void setProbedType( const zypp::repo::ServiceType & type_r ) const
    {
      if ( _type == zypp::repo::ServiceType::NONE
           && type_r != zypp::repo::ServiceType::NONE )
      {
        // lazy init!
        const_cast<ServiceInfoSharedData*>(this)->_type = type_r;
      }
    }

  protected:
    void bindVariables () override;

  private:
    friend ServiceInfoSharedData * zypp::rwcowClone<ServiceInfoSharedData>( const ServiceInfoSharedData * rhs );

    /** clone for RWCOW_pointer */
    ServiceInfoSharedData( const ServiceInfoSharedData &) = default;
    ServiceInfoSharedData *clone() const override;
  };

}


#endif // ZYPP_NG_SERVICE_INFO_SHARED_DATA_H_INCLUDED
