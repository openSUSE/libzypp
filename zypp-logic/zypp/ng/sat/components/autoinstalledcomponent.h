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
#ifndef ZYPP_NG_SAT_COMPONENTS_AUTOINSTALLEDCOMPONENT_H_INCLUDED
#define ZYPP_NG_SAT_COMPONENTS_AUTOINSTALLEDCOMPONENT_H_INCLUDED

#include "poolcomponents.h"
#include <zypp/ng/sat/queue.h>
#include <zypp/ng/idstring.h>

namespace zyppng::sat {

  /**
   * \brief Component managing the list of autoinstalled solvables.
   *
   * It provides hints about whether a solvable was installed manually by the user
   * or automatically by the solver.
   */
  class AutoInstalledComponent : public IPoolComponent {
    public:
      AutoInstalledComponent() = default;

      ComponentStage stage() const override { return ComponentStage::Policy; }

      /** Get ident list of all autoinstalled solvables. */
      const StringQueue & autoInstalled() const { return _autoinstalled; }

      /** Set ident list of all autoinstalled solvables. */
      void setAutoInstalled( StringQueue autoInstalled_r ) { _autoinstalled = std::move(autoInstalled_r); }

      bool isOnSystemByUser( IdString ident_r ) const { return !_autoinstalled.contains( ident_r.id() ); }
      bool isOnSystemByAuto( IdString ident_r ) const { return _autoinstalled.contains( ident_r.id() ); }
      //@}

      void onRepoRemoved( Pool & pool, detail::RepoIdType id ) override;

    private:
      StringQueue _autoinstalled;
  };

} // namespace zyppng::sat

#endif
