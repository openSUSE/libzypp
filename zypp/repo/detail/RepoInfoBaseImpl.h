/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/repo/detail/RepoInfoBaseImpl.h
 *
*/

#include <zypp-core/TriBool.h>
#include <zypp/repo/RepoInfoBase.h>
#include <zypp/repo/RepoVariables.h>

namespace zypp::repo
{

  ///////////////////////////////////////////////////////////////////
  /// \class RepoInfoBase::Impl
  /// \brief RepoInfoBase data
  ///////////////////////////////////////////////////////////////////
  struct RepoInfoBase::Impl
  {
    Impl(const Impl &) = default;
    Impl(Impl &&) = delete;
    Impl &operator=(const Impl &) = default;
    Impl &operator=(Impl &&) = delete;
    Impl(zyppng::ContextBaseRef &&context);

    Impl( zyppng::ContextBaseRef &&context, const std::string & alias_r );

    virtual ~Impl() = default;

  public:
    zyppng::ContextBaseRef _ctx;
    TriBool	_enabled;
    TriBool	_autorefresh;
    std::string	_alias;
    std::string	_escaped_alias;
    RepoVariablesReplacedString _name;
    Pathname	_filepath;

  public:

    void setAlias( const std::string & alias_r );

  private:
    friend Impl * rwcowClone<Impl>( const Impl * rhs );

    /** clone for RWCOW_pointer */
    virtual Impl *clone() const;
  };
}
