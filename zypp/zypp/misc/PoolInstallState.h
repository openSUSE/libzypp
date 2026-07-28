/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/misc/PoolInstallState.h
 *
 * Structured install-state flags for pool items and selectables.
 *
 * Unlike zypper's two-character status indicator (designed for terminal
 * display), \ref PoolInstallStateFlags are composable bit flags suitable for
 * programmatic inspection — e.g. by an MCP tool or a REST API.
 *
 * \note This is distinct from \ref ResStatus which describes the solver's
 *       internal transaction state. \ref PoolInstallStateFlags describe
 *       the user-visible install state of a resolvable in the pool.
 */
#ifndef ZYPP_MISC_POOLINSTALLSTATE_H
#define ZYPP_MISC_POOLINSTALLSTATE_H

#include <zypp-core/Globals.h>
#include <zypp-core/base/Flags.h>
#include <zypp/PoolItem.h>

namespace zypp {
  namespace ui { class Selectable; }
}

///////////////////////////////////////////////////////////////////
namespace zypp::misc
{ /////////////////////////////////////////////////////////////////

  /**
   * Install-state flags for a pool item or selectable.
   *
   * \c NotInstalled (zero) is the base state — nothing installed, nothing special.
   * All other flags are additive modifiers on top of the install state.
   *
   * Flags compose naturally:
   * - \c AutoInstalled | \c Locked — auto-installed dependency that is locked.
   * - \c OtherVersionInstalled | \c UserInstalled — user-installed, but a
   *   different version than any available candidate.
   *
   * Testing for "is anything installed":
   * \code
   * flags & ( PoolInstallState::AutoInstalled | PoolInstallState::UserInstalled )
   * \endcode
   *
   * \note \c OtherVersionInstalled has nothing to do with vendor/source mismatch.
   *       It means the installed version is not identical to any available candidate
   *       in the current pool (which may be repo-filtered by the caller). It is
   *       always combined with \c AutoInstalled or \c UserInstalled so callers know
   *       how the other version got there.
   */
  enum class PoolInstallState
  {
    NotInstalled          = 0,        ///< Not installed (base/default state)
    AutoInstalled         = (1 << 0), ///< Installed as a dependency (auto)
    UserInstalled         = (1 << 1), ///< Explicitly installed by the user
    OtherVersionInstalled = (1 << 2), ///< Installed, but no identical candidate available.
                                       ///  Always combined with \c AutoInstalled or \c UserInstalled.
    Satisfied             = (1 << 3), ///< Pseudo-installed (patch/pattern) and satisfied
    Broken                = (1 << 4), ///< Pseudo-installed and requirements are not met
    Locked                = (1 << 5), ///< Locked — solver will not change this item
    Retracted             = (1 << 6), ///< Retracted (PTF / blacklisted)
  };
  ZYPP_DECLARE_FLAGS_AND_OPERATORS( PoolInstallStateFlags, PoolInstallState );

  /** Compute install-state flags for a \ref PoolItem.
   *
   * For the \ref PoolInstallState::OtherVersionInstalled flag, the selectable
   * is looked up internally — callers do not need to provide it.
   */
  ZYPP_API PoolInstallStateFlags poolInstallState( const PoolItem & pi );

  /** Compute install-state flags for a \ref ui::Selectable.
   *
   * \ref PoolInstallState::OtherVersionInstalled is set when the installed
   * object has no identical candidate available in the current pool. If the
   * caller operates on a repo-filtered pool this naturally reflects the
   * zypper "foreign installed" (`v`) case — no separate flag is needed.
   */
  ZYPP_API PoolInstallStateFlags poolInstallState( const ui::Selectable & sel );

  /////////////////////////////////////////////////////////////////
} // namespace zypp::misc
///////////////////////////////////////////////////////////////////

#endif // ZYPP_MISC_POOLINSTALLSTATE_H
