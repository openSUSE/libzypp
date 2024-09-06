/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file       zypp/repo/RepoInfoBase.cc
 *
 */
#include <iostream>

#include <zypp/ZConfig.h>
#include <zypp/zypp_detail/ZYppImpl.h>
#include <zypp/repo/detail/RepoInfoBaseImpl.h>
#include <zypp/repo/RepoVariables.h>

#include <zypp/repo/RepoInfoBase.h>
#include <zypp/TriBool.h>
#include <zypp/Pathname.h>

#include <zypp/ng/ContextBase>

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace repo
  {

  RepoInfoBase::Impl::Impl( zyppng::ContextBaseRef &&context )
    : _ctx( std::move(context) )
    , _enabled( indeterminate )
    , _autorefresh( indeterminate )
  {}

  RepoInfoBase::Impl::Impl(  zyppng::ContextBaseRef &&context, const std::string & alias_r )
    : _ctx( std::move(context) )
    , _enabled( indeterminate )
    , _autorefresh( indeterminate )
  { setAlias( alias_r ); }


  void RepoInfoBase::Impl::setAlias( const std::string & alias_r )
  {
    _alias = _escaped_alias = alias_r;
    // replace slashes with underscores
    str::replaceAll( _escaped_alias, "/", "_" );
  }

  RepoInfoBase::Impl * RepoInfoBase::Impl::clone() const
  { return new Impl( *this ); }

  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //    CLASS NAME : RepoInfoBase
  //
  ///////////////////////////////////////////////////////////////////

  RepoInfoBase::RepoInfoBase(Impl &pimpl  ) : _pimpl( &pimpl )
  {}

  RepoInfoBase::RepoInfoBase( zyppng::ContextBaseRef context )
    : _pimpl( new Impl( std::move(context) ) )
  { }

  RepoInfoBase::RepoInfoBase( zyppng::ContextBaseRef context, const std::string &alias )
    : _pimpl( new Impl( std::move(context), alias ) )
  { }

  zyppng::ContextBaseRef RepoInfoBase::context() const
  { return _pimpl->_ctx; }

  RepoInfoBase::RepoInfoBase()
    : RepoInfoBase( zypp_detail::GlobalStateHelper::context() )
  { /* LEGACY, DO NOT ADD CODE HERE */ }

  RepoInfoBase::RepoInfoBase(const std::string & alias)
    : RepoInfoBase( zypp_detail::GlobalStateHelper::context(), alias )
  { /* LEGACY, DO NOT ADD CODE HERE */ }

  RepoInfoBase::~RepoInfoBase()
  {}

  void RepoInfoBase::setEnabled( bool enabled )
  { _pimpl->_enabled = enabled; }

  void RepoInfoBase::setAutorefresh( bool autorefresh )
  { _pimpl->_autorefresh = autorefresh; }

  void RepoInfoBase::setAlias( const std::string &alias )
  { _pimpl->setAlias(alias); }

  void RepoInfoBase::setName( const std::string &name )
  { _pimpl->_name.raw() = name; }

  void RepoInfoBase::setFilepath( const Pathname &filepath )
  { _pimpl->_filepath = filepath; }

  // true by default (if not set by setEnabled())
  bool RepoInfoBase::enabled() const
  { return indeterminate(_pimpl->_enabled) ? true : (bool) _pimpl->_enabled; }

  // false by default (if not set by setAutorefresh())
  bool RepoInfoBase::autorefresh() const
  { return indeterminate(_pimpl->_autorefresh) ? false : (bool) _pimpl->_autorefresh; }

  std::string RepoInfoBase::alias() const
  { return _pimpl->_alias; }

  std::string RepoInfoBase::escaped_alias() const
  { return _pimpl->_escaped_alias; }

  std::string RepoInfoBase::name() const
  {
    if ( rawName().empty() )
      return alias();
    return repo::RepoVariablesStringReplacer()( rawName() );
  }

  std::string RepoInfoBase::rawName() const
  { return _pimpl->_name.raw(); }

  std::string RepoInfoBase::label() const
  {
    if ( _pimpl->_ctx && _pimpl->_ctx->config().repoLabelIsAlias() )
      return alias();
    return name();
  }

  Pathname RepoInfoBase::filepath() const
  { return _pimpl->_filepath; }


  std::ostream & RepoInfoBase::dumpOn( std::ostream & str ) const
  {
    str << "--------------------------------------" << std::endl;
    str << "- alias       : " << alias() << std::endl;
    if ( ! rawName().empty() )
      str << "- name        : " << rawName() << std::endl;
    str << "- enabled     : " << enabled() << std::endl;
    str << "- autorefresh : " << autorefresh() << std::endl;

    return str;
  }

  std::ostream & RepoInfoBase::dumpAsIniOn( std::ostream & str ) const
  {
    // we save the original data without variable replacement
    str << "[" << alias() << "]" << endl;
    if ( ! rawName().empty() )
      str << "name=" << rawName() << endl;
    str << "enabled=" << (enabled() ? "1" : "0") << endl;
    str << "autorefresh=" << (autorefresh() ? "1" : "0") << endl;

    return str;
  }

  std::ostream & RepoInfoBase::dumpAsXmlOn( std::ostream & str, const std::string & content ) const
  {
    return str << "<!-- there's no XML representation of RepoInfoBase -->" << endl;
  }

  std::ostream & operator<<( std::ostream & str, const RepoInfoBase & obj )
  {
    return obj.dumpOn(str);
  }

  } // namespace repo
  ///////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
