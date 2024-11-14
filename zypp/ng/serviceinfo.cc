/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file       zypp/ServiceInfo.cc
 *
 */
#include <ostream>
#include <iostream>

#include <zypp/zypp_detail/ZYppImpl.h>
#include <zypp/base/String.h>
#include <zypp-core/base/DefaultIntegral>
#include <zypp/parser/xml/XmlEscape.h>

#include <zypp/RepoInfo.h>
#include <zypp/ServiceInfo.h>
#include <zypp/ng/serviceinfoshareddata.h>

#include <zypp/ng/ContextBase>

using std::endl;
using zypp::xml::escape;

namespace zyppng
{

  ServiceInfoSharedData::ServiceInfoSharedData( ContextBaseRef &&context ) : repo::RepoInfoBaseSharedData( std::move(context) )
  { ServiceInfoSharedData::bindVariables(); }

  ServiceInfoSharedData::ServiceInfoSharedData( ContextBaseRef &&context, const std::string &alias )
    : repo::RepoInfoBaseSharedData( std::move(context), alias )
  { ServiceInfoSharedData::bindVariables(); }

  ServiceInfoSharedData::ServiceInfoSharedData( ContextBaseRef &&context, const std::string &alias, const zypp::Url &url_r )
    : repo::RepoInfoBaseSharedData( std::move(context), alias )
    , _url(url_r)
  { ServiceInfoSharedData::bindVariables(); }

  void ServiceInfoSharedData::bindVariables()
  {
    repo::RepoInfoBaseSharedData::bindVariables();
    if ( _ctx ) {
      _url.setTransformator( repo::RepoVariablesUrlReplacer( zypp::repo::RepoVarRetriever( *_ctx.get() ) ) );
    } else {
      _url.setTransformator( repo::RepoVariablesUrlReplacer( nullptr ) );
    }
  }

  ServiceInfoSharedData *ServiceInfoSharedData::clone() const
  {
    auto *n =  new ServiceInfoSharedData(*this);
    n->bindVariables ();
    return n;
  }
  ///////////////////////////////////////////////////////////////////
  //
  //  CLASS NAME : ServiceInfo::Impl
  //
  //////////////////////////////////////////////////////////////////

  ServiceInfo::ServiceInfo( zyppng::ContextBaseRef ctx ) : repo::RepoInfoBase( ( *new ServiceInfoSharedData( std::move(ctx) )) ) {}

  ServiceInfo::ServiceInfo( zyppng::ContextBaseRef ctx, const std::string & alias )
    : repo::RepoInfoBase( ( *new ServiceInfoSharedData( std::move(ctx), alias )) )
  {}

  ServiceInfo::ServiceInfo( zyppng::ContextBaseRef ctx, const std::string & alias, const zypp::Url & url )
    : repo::RepoInfoBase( ( *new ServiceInfoSharedData( std::move(ctx), alias, url )) )
  {}

  ServiceInfo::~ServiceInfo()
  {}

  zypp::Url ServiceInfo::url() const			// Variables replaced
  { return pimpl()->_url.transformed(); }

  zypp::Url ServiceInfo::rawUrl() const		// Raw
  { return pimpl()->_url.raw(); }

  void ServiceInfo::setUrl( const zypp::Url& url )	// Raw
  { pimpl()->_url.raw() = url; }

  zypp::repo::ServiceType ServiceInfo::type() const				{ return pimpl()->_type; }
  void ServiceInfo::setType( const zypp::repo::ServiceType & type )		{ pimpl()->_type = type; }
  void ServiceInfo::setProbedType( const zypp::repo::ServiceType &t ) const	{ pimpl()->setProbedType( t ); }

  zypp::Date::Duration ServiceInfo::ttl() const                         { return pimpl()->_ttl; }
  void ServiceInfo::setTtl( zypp::Date::Duration ttl_r )		{ pimpl()->_ttl = ttl_r; }
  void ServiceInfo::setProbedTtl( zypp::Date::Duration ttl_r ) const	{ const_cast<ServiceInfo*>(this)->setTtl( ttl_r ); }

  zypp::Date ServiceInfo::lrf() const				{ return pimpl()->_lrf; }
  void ServiceInfo::setLrf( zypp::Date lrf_r )			{ pimpl()->_lrf = lrf_r; }

  bool ServiceInfo::reposToEnableEmpty() const						{ return pimpl()->_reposToEnable.empty(); }
  ServiceInfo::ReposToEnable::size_type ServiceInfo::reposToEnableSize() const		{ return pimpl()->_reposToEnable.size(); }
  ServiceInfo::ReposToEnable::const_iterator ServiceInfo::reposToEnableBegin() const	{ return pimpl()->_reposToEnable.begin(); }
  ServiceInfo::ReposToEnable::const_iterator ServiceInfo::reposToEnableEnd() const	{ return pimpl()->_reposToEnable.end(); }

  bool ServiceInfo::repoToEnableFind( const std::string & alias_r ) const
  { return( pimpl()->_reposToEnable.find( alias_r ) != pimpl()->_reposToEnable.end() ); }

  void ServiceInfo::addRepoToEnable( const std::string & alias_r )
  {
    pimpl()->_reposToEnable.insert( alias_r );
    pimpl()->_reposToDisable.erase( alias_r );
  }

  void ServiceInfo::delRepoToEnable( const std::string & alias_r )
  { pimpl()->_reposToEnable.erase( alias_r ); }

  void ServiceInfo::clearReposToEnable()
  { pimpl()->_reposToEnable.clear(); }


  bool ServiceInfo::reposToDisableEmpty() const						{ return pimpl()->_reposToDisable.empty(); }
  ServiceInfo::ReposToDisable::size_type ServiceInfo::reposToDisableSize() const	{ return pimpl()->_reposToDisable.size(); }
  ServiceInfo::ReposToDisable::const_iterator ServiceInfo::reposToDisableBegin() const	{ return pimpl()->_reposToDisable.begin(); }
  ServiceInfo::ReposToDisable::const_iterator ServiceInfo::reposToDisableEnd() const	{ return pimpl()->_reposToDisable.end(); }

  bool ServiceInfo::repoToDisableFind( const std::string & alias_r ) const
  { return( pimpl()->_reposToDisable.find( alias_r ) != pimpl()->_reposToDisable.end() ); }

  void ServiceInfo::addRepoToDisable( const std::string & alias_r )
  {
    pimpl()->_reposToDisable.insert( alias_r );
    pimpl()->_reposToEnable.erase( alias_r );
  }

  void ServiceInfo::delRepoToDisable( const std::string & alias_r )
  { pimpl()->_reposToDisable.erase( alias_r ); }

  void ServiceInfo::clearReposToDisable()
  { pimpl()->_reposToDisable.clear(); }


  const ServiceInfo::RepoStates & ServiceInfo::repoStates() const	{ return pimpl()->_repoStates; }
  void ServiceInfo::setRepoStates( RepoStates newStates_r )		{ swap( pimpl()->_repoStates, newStates_r ); }

  std::ostream & ServiceInfo::dumpAsIniOn( std::ostream & str ) const
  {
    RepoInfoBase::dumpAsIniOn(str)
      << "url = " << zypp::hotfix1050625::asString( rawUrl() ) << endl
      << "type = " << type() << endl;

    if ( ttl() )
      str << "ttl_sec = " << ttl() << endl;

    if ( lrf() )
      str << "lrf_dat = " << lrf().asSeconds() << endl;

    if ( ! repoStates().empty() )
    {
      unsigned cnt = 0U;
      for ( const auto & el : repoStates() )
      {
        std::string tag( "repo_" );
        tag += zypp::str::numstring( ++cnt );
        const RepoState & state( el.second );

        str << tag << "=" << el.first << endl
            << tag << "_enabled=" << state.enabled << endl
            << tag << "_autorefresh=" << state.autorefresh << endl;
        if ( state.priority != RepoInfo::defaultPriority() )
          str
            << tag << "_priority=" << state.priority << endl;
      }
    }

    if ( ! reposToEnableEmpty() )
      str << "repostoenable = " << zypp::str::joinEscaped( reposToEnableBegin(), reposToEnableEnd() ) << endl;
    if ( ! reposToDisableEmpty() )
      str << "repostodisable = " << zypp::str::joinEscaped( reposToDisableBegin(), reposToDisableEnd() ) << endl;
    return str;
  }

  std::ostream & ServiceInfo::dumpAsXmlOn( std::ostream & str, const std::string & content ) const
  {
    str
      << "<service"
      << " alias=\"" << escape(alias()) << "\""
      << " name=\"" << escape(name()) << "\""
      << " enabled=\"" << enabled() << "\""
      << " autorefresh=\"" << autorefresh() << "\""
      << " url=\"" << escape(url().asString()) << "\""
      << " type=\"" << type().asString() << "\""
      << " ttl_sec=\"" << ttl() << "\"";

    if (content.empty())
      str << "/>" << endl;
    else
      str << ">" << endl << content << "</service>" << endl;

    return str;
  }

  ServiceInfoSharedData *ServiceInfo::pimpl()
  {
    return static_cast<ServiceInfoSharedData*>(_pimpl.get());
  }

  const ServiceInfoSharedData *ServiceInfo::pimpl() const
  {
    return static_cast<const ServiceInfoSharedData*>(_pimpl.get());
  }


  std::ostream & operator<<( std::ostream& str, const ServiceInfo &obj )
  {
    return obj.dumpAsIniOn(str);
  }


///////////////////////////////////////////////////////////////////////////////

} // namespace zyppng
///////////////////////////////////////////////////////////////////////////////
