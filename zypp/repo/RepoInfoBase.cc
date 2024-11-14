/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/repo/RepoInfoBase.cc
 *
*/
#include "RepoInfoBase.h"
#include <zypp/ng/repo/repoinfobase.h>

ZYPP_BEGIN_LEGACY_API

namespace zypp::repo {

  RepoInfoBase::RepoInfoBase( )
  {}

  RepoInfoBase::~RepoInfoBase()
  {}

  void RepoInfoBase::setEnabled( bool enabled )
  { pimpl().setEnabled(enabled); }

  void RepoInfoBase::setAutorefresh( bool autorefresh )
  { pimpl().setAutorefresh(autorefresh); }

  void RepoInfoBase::setAlias( const std::string &alias )
  { pimpl().setAlias(alias); }

  void RepoInfoBase::setName( const std::string &name )
  { pimpl().setName(name); }

  void RepoInfoBase::setFilepath( const zypp::Pathname &filepath )
  { pimpl().setFilepath(filepath); }

  bool RepoInfoBase::enabled() const
  { return pimpl().enabled(); }

  bool RepoInfoBase::autorefresh() const
  { return pimpl().autorefresh(); }

  std::string RepoInfoBase::alias() const
  { return pimpl().alias(); }

  std::string RepoInfoBase::escaped_alias() const
  { return pimpl().escaped_alias(); }

  std::string RepoInfoBase::name() const
  { return pimpl().name(); }

  std::string RepoInfoBase::rawName() const
  { return pimpl().rawName(); }

  std::string RepoInfoBase::label() const
  { return pimpl().label(); }

  zypp::Pathname RepoInfoBase::filepath() const
  { return pimpl().filepath(); }

  std::ostream & RepoInfoBase::dumpOn( std::ostream & str ) const
  { return pimpl().dumpOn(str); }

  std::ostream & RepoInfoBase::dumpAsIniOn( std::ostream & str ) const
  { return pimpl().dumpAsIniOn(str); }

  std::ostream & RepoInfoBase::dumpAsXmlOn( std::ostream & str, const std::string & content ) const
  { return pimpl().dumpAsXmlOn (str, content); }

  std::ostream & operator<<( std::ostream & str, const RepoInfoBase & obj )
  {
    return obj.dumpOn(str);
  }
}

ZYPP_END_LEGACY_API
