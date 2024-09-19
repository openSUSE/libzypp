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

#ifndef ZYPP_NG_REPOINFOBASESHAREDDATA_INCLUDED
#define ZYPP_NG_REPOINFOBASESHAREDDATA_INCLUDED

#include <zypp/ng/contextbase.h>
#include <zypp-core/TriBool.h>
#include <zypp/repo/RepoInfoBase.h>
#include <zypp/ng/repo/repovariables.h>

namespace zyppng::repo
{

  /*!
   * \brief Contains shared data for Service and RepoInfo types
   */
  struct RepoInfoBaseSharedData
  {
    RepoInfoBaseSharedData(const RepoInfoBaseSharedData &) = default;
    RepoInfoBaseSharedData(RepoInfoBaseSharedData &&) = delete;
    RepoInfoBaseSharedData &operator=(const RepoInfoBaseSharedData &) = default;
    RepoInfoBaseSharedData &operator=(RepoInfoBaseSharedData &&) = delete;

    RepoInfoBaseSharedData( zyppng::ContextBaseRef &&context );
    RepoInfoBaseSharedData( zyppng::ContextBaseRef &&context, const std::string & alias_r );

    virtual ~RepoInfoBaseSharedData() = default;

  public:
    zyppng::ContextBaseRef _ctx;
    zypp::TriBool	_enabled;
    zypp::TriBool	_autorefresh;
    std::string	_alias;
    std::string	_escaped_alias;
    RepoVariablesReplacedString _name;
    zypp::Pathname	_filepath;

  public:
    void setAlias( const std::string & alias_r );
    void switchContext( ContextBaseRef ctx );

  protected:
    virtual void bindVariables();

  private:
    friend RepoInfoBaseSharedData * zypp::rwcowClone<RepoInfoBaseSharedData>( const RepoInfoBaseSharedData * rhs );

    /** clone for RWCOW_pointer */
    virtual RepoInfoBaseSharedData *clone() const;
  };
}

#endif //ZYPP_NG_REPOINFOBASESHAREDDATA_INCLUDED
