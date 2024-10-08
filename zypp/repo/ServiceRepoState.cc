/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "ServiceRepoState.h"

#include <zypp/ng/repoinfo.h>

namespace zypp {

  ServiceRepoState::ServiceRepoState( const zyppng::RepoInfo &repo_r )
    : enabled( repo_r.enabled() ), autorefresh( repo_r.autorefresh() ), priority( repo_r.priority() )
  {}

ZYPP_BEGIN_LEGACY_API
  ServiceRepoState::ServiceRepoState(const RepoInfo &repo_r)
    : ServiceRepoState(repo_r.ngRepoInfo()) {}
ZYPP_END_LEGACY_API

  ServiceRepoState::ServiceRepoState()
    : enabled(false), autorefresh(true),
      priority(zyppng::RepoInfo::defaultPriority()) {}
} // namespace zypp
