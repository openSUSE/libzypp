/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
module;

module zyppng;

import :sat_components_autoinstalledcomponent;
import :sat_pool;

namespace zyppng::sat {

  void AutoInstalledComponent::onRepoRemoved( Pool & pool, detail::RepoIdType id )
  {
    if ( pool.isSystemRepo( id ) )
      _autoinstalled.clear();
  }

  void AutoInstalledComponent::onReset( Pool & )
  {
    _autoinstalled.clear();
  }

} // namespace zyppng::sat
