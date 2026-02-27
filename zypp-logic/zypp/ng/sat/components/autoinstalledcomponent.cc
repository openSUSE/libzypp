/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "autoinstalledcomponent.h"
#include <zypp/ng/sat/poolbase.h>

namespace zyppng::sat {

  void AutoInstalledComponent::onRepoRemoved( PoolBase & pool, detail::RepoIdType id )
  {
    if ( pool.isSystemRepo( id ) )
      _autoinstalled.clear();
  }

} // namespace zyppng::sat
