/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "ResFilters.h"
#include <zypp/ng/repoinfo.h>

namespace zypp {
  resfilter::ByRepository::ByRepository(Repository repository_r)
    : _alias(repository_r.ngInfo() ? repository_r.ngInfo()->alias() : "" ) {}
  resfilter::ByRepository::ByRepository(std::string alias_r)
    : _alias(std::move(alias_r)) {}
  bool resfilter::ByRepository::operator()(const ResObject::constPtr &p) const {
    return (p->ngRepoInfo() ? p->ngRepoInfo()->alias() : "" ) == _alias;
  }
} // namespace zypp
