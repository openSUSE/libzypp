/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include <iostream>
#include <map>
#include <zypp/base/NamedValue.h>
#include <zypp/repo/RepoException.h>
#include "ServiceType.h"

namespace zypp
{
  namespace repo
  {
  ///////////////////////////////////////////////////////////////////
  namespace
  {
    static NamedValue<ServiceType::Type> & table()
    {
      static NamedValue<ServiceType::Type> & _t( *new NamedValue<ServiceType::Type> );
      if ( _t.empty() )
      {
        _t( ServiceType::RIS_e )        | "ris"       |"RIS"|"nu"|"NU";
        _t( ServiceType::PLUGIN_e )     | "plugin"    |"PLUGIN";
        _t( ServiceType::NONE_e )       | "N/A"       |"NONE"|"none";
      }
      return _t;
    }
  } // namespace
  ///////////////////////////////////////////////////////////////////

  const ServiceType ServiceType::RIS(ServiceType::RIS_e);
  const ServiceType ServiceType::NONE(ServiceType::NONE_e);
  const ServiceType ServiceType::PLUGIN(ServiceType::PLUGIN_e);

  ServiceType::ServiceType(const std::string & strval_r)
    : _type(parse(strval_r))
  {}

  ServiceType::Type ServiceType::parse(const std::string & strval_r)
  {
    ServiceType::Type type;
    if ( ! table().getValue( str::toLower( strval_r ), type ) )
    {
      ZYPP_THROW( RepoUnknownTypeException( "Unknown service type '" + strval_r + "'") );
    }
    return type;
  }

  const std::string & ServiceType::asString() const
  {
    return table().getName( _type );
  }


  } // ns repo
} // ns zypp

// vim: set ts=2 sts=2 sw=2 et ai:
