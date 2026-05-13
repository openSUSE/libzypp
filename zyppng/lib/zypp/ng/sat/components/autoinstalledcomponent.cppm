/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/

module;
#include <utility>  // std::move

export module zyppng:sat_components_autoinstalledcomponent;

import :sat_components_poolcomponents;
import :sat_queue;   // StringQueue
import :idstring;    // IdString

export namespace zyppng::sat {

  /**
   * \brief Component managing the list of autoinstalled solvables.
   *
   * It provides hints about whether a solvable was installed manually by the user
   * or automatically by the solver.
   */
  class AutoInstalledComponent : public IPoolComponent {
    public:
      AutoInstalledComponent() = default;

      /** Get ident list of all autoinstalled solvables. */
      const StringQueue & autoInstalled() const { return _autoinstalled; }

      /** Set ident list of all autoinstalled solvables. */
      void setAutoInstalled( StringQueue autoInstalled_r ) { _autoinstalled = std::move(autoInstalled_r); }

      bool isOnSystemByUser( IdString ident_r ) const { return !_autoinstalled.contains( ident_r.id() ); }
      bool isOnSystemByAuto( IdString ident_r ) const { return _autoinstalled.contains( ident_r.id() ); }
      //@}

      void onRepoRemoved( Pool & pool, detail::RepoIdType id ) override;
      void onReset( Pool & pool ) override;

    private:
      StringQueue _autoinstalled;
  };

} // namespace zyppng::sat

