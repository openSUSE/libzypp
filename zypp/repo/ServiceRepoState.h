/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/repo/ServiceInfo.h
 *
 */
#ifndef ZYPP_REPO_SERVICE_REPO_STATE_H
#define ZYPP_REPO_SERVICE_REPO_STATE_H

#include <zypp/RepoInfo.h>

namespace zyppng {
  class RepoInfo;
}

namespace zypp
{

  /** \name The original repo state as defined by the repoindex.xml upon last refresh.
   *
   * This state is remembered to detect any user modifications applied to the repos.
   * It may not be available for all repos or in plugin services. In this case all
   * changes requested by a service refresh are applied unconditionally.
   */
  //@{
  struct ZYPP_API ServiceRepoState
  {
    bool	enabled;
    bool	autorefresh;
    unsigned	priority;

    ServiceRepoState();

    ServiceRepoState( const RepoInfo &repo_r ) ZYPP_INTERNAL_DEPRECATE;

    ServiceRepoState( const zyppng::RepoInfo &repo_r );

    bool operator==( const ServiceRepoState & rhs ) const
    { return( enabled==rhs.enabled && autorefresh==rhs.autorefresh && priority==rhs.priority ); }
    bool operator!=( const ServiceRepoState & rhs ) const
    { return ! operator==( rhs ); }
    friend std::ostream & operator<<( std::ostream & str, const ServiceRepoState & obj );
  };

}

#endif
