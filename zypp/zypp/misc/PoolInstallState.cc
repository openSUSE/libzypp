/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/misc/PoolInstallState.cc
 *
*/
#include "PoolInstallState.h"

#include <zypp/PoolItem.h>
#include <zypp/ResTraits.h>
#include <zypp/ResStatus.h>
#include <zypp/ui/Selectable.h>

namespace zypp::misc
{
  PoolInstallStateFlags poolInstallState( const PoolItem & pi )
  {
    using S = PoolInstallState;
    PoolInstallStateFlags flags;   // NotInstalled = 0

    const ResStatus & status = pi.status();

    // ── Pseudo-installed kinds (patch, pattern) ───────────────────────────────
    // These use validate() rather than isInstalled() to determine state.
    if ( traits::isPseudoInstalled( pi->kind() ) )
    {
      switch ( status.validate() )
      {
        case ResStatus::SATISFIED:  flags |= S::Satisfied; break;
        case ResStatus::BROKEN:     flags |= S::Broken;    break;
        case ResStatus::UNDETERMINED:	// [[fallthrough]]
        case ResStatus::NONRELEVANT:	break;
      }
    }
    else
    // ── Regular resolvables ───────────────────────────────────────────────────
    {
      if ( status.isInstalled() )
      {
        flags |= pi.identIsAutoInstalled() ? S::AutoInstalled : S::UserInstalled;
      }
      else
      {
        // Check if a different version of this package is installed.
        ui::Selectable::Ptr sel = ui::Selectable::get( pi );
        if ( sel && sel->hasInstalledObj() )
        {
          if ( sel->identicalInstalled( pi ) )
            flags |= pi.identIsAutoInstalled() ? S::AutoInstalled : S::UserInstalled;
          else
          {
            // Different version installed — combine with auto/user distinction
            // so the caller knows both that the version differs AND how it got there.
            flags |= S::OtherVersionInstalled;
            flags |= sel->identIsAutoInstalled() ? S::AutoInstalled : S::UserInstalled;
          }
        }
        // else: NotInstalled (zero) — nothing to set
      }
    }

    // ── Modifiers — independent of install state ──────────────────────────────
    if ( status.isLocked()  ) flags |= S::Locked;
    if ( pi.isBlacklisted() ) flags |= S::Retracted;

    return flags;
  }

  PoolInstallStateFlags poolInstallState( const ui::Selectable & sel )
  {
    using S = PoolInstallState;
    PoolInstallStateFlags flags;   // NotInstalled = 0

    // ── Pseudo-installed kinds ────────────────────────────────────────────────
    if ( traits::isPseudoInstalled( sel.kind() ) )
    {
      switch ( sel.theObj().status().validate() )
      {
        case ResStatus::SATISFIED:  flags |= S::Satisfied; break;
        case ResStatus::BROKEN:     flags |= S::Broken;    break;
        default:                                            break;
      }
    }
    else
    // ── Regular resolvables ───────────────────────────────────────────────────
    {
      if ( sel.hasInstalledObj() )
      {
        flags |= sel.identIsAutoInstalled() ? S::AutoInstalled : S::UserInstalled;

        // OtherVersionInstalled: no available candidate is identical to what is
        // installed. In a repo-filtered pool this is the "foreign installed" (v)
        // case from zypper — no separate flag required.
        if ( !sel.identicalAvailable( sel.installedObj() ) )
          flags |= S::OtherVersionInstalled;
      }
      // else: NotInstalled (zero) — nothing to set
    }

    // ── Modifiers ─────────────────────────────────────────────────────────────
    if ( sel.locked()                  ) flags |= S::Locked;
    if ( sel.hasBlacklistedInstalled() ) flags |= S::Retracted;

    return flags;
  }

} // namespace zypp::misc
