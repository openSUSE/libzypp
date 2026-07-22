/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/misc/TestcaseSetup.h
 *
*/

#ifndef ZYPP_MISC_TESTCASESETUP_H
#define ZYPP_MISC_TESTCASESETUP_H

#include <zypp/Arch.h>
#include <zypp-core/Globals.h>
#include <zypp-core/base/Flags.h>
#include <zypp/Locale.h>
#include <zypp-core/Pathname.h>
#include <zypp/ResolverFocus.h>
#include <zypp-core/Url.h>
#include <zypp-core/base/PtrTypes.h>
#include <zypp/base/SetTracker.h>
#include <zypp/sat/Queue.h>
#include <zypp/target/modalias/Modalias.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace zypp {
  class RepoManager;
  class ResPool;
}

namespace zypp::misc::testcase
{

  enum class TestcaseRepoType {
    Helix,
    Testtags,
    Url
  };

  struct RepoDataImpl;
  struct ForceInstallImpl;
  struct TestcaseSetupImpl;

  class RepoData {
  public:
    RepoData ();
    ~RepoData ();
    RepoData ( RepoDataImpl &&data );
    TestcaseRepoType type() const;
    const std::string &alias() const;
    uint priority() const;
    const std::string &path() const;

    const RepoDataImpl &data() const;
    RepoDataImpl &data();
  private:
    RWCOW_pointer<RepoDataImpl> _pimpl;
  };

  class ZYPP_API_DEPTESTOMATIC ForceInstall {
  public:
    ForceInstall ();
    ~ForceInstall ();
    ForceInstall ( ForceInstallImpl &&data );
    const std::string &channel () const;
    const std::string &package () const;
    const std::string &kind () const;

    const ForceInstallImpl &data() const;
    ForceInstallImpl &data();
  private:
    RWCOW_pointer<ForceInstallImpl> _pimpl;
  };

  /**
   * Bits controlling how much of the testcase setup is applied.
   *
   * The zero-flag value (AS_REPOS_ONLY) replicates the historic behaviour of
   * \ref applySetup(RepoManager&): only architecture, repos and hardware-info
   * are applied.  Use \ref AS_ALL to apply everything — this is what
   * deptestomatic and testcase-runner want for full fidelity.
   * Use \ref AS_UNIVERSE when you want the package environment but not the
   * solver opinions from the captured session (e.g. MCP tool).
   */
  enum ApplySetupFlag {
    AS_REPOS_ONLY    = 0,        ///< Only arch, repos, hardware-info (historic default)
    AS_SOLVER_FLAGS  = (1 << 0), ///< Resolver flags, focus, DUP flags
    AS_LOCALES       = (1 << 1), ///< initRequestedLocales + setRequestedLocales
    AS_AUTOINSTALLED = (1 << 2), ///< setAutoInstalled
    AS_VENDOR_LISTS  = (1 << 3), ///< VendorAttr vendor-equivalence lists
    AS_MODALIAS      = (1 << 4), ///< Modalias list
    AS_MULTIVERSION  = (1 << 5), ///< ZConfig multiversion spec
    AS_LOCKS         = (1 << 6), ///< lock/keep nodes extracted from trial nodes
    /** Package universe without solver opinions: repos, locales, autoinstalled,
     *  vendor lists, modalias, multiversion, locks — but NOT solver flags.
     *  Use this when the solver will be driven by a new request (e.g. MCP tool)
     *  rather than replaying the original session. */
    AS_UNIVERSE      = AS_LOCALES | AS_AUTOINSTALLED | AS_VENDOR_LISTS | AS_MODALIAS | AS_MULTIVERSION | AS_LOCKS,
    AS_ALL           = ~0        ///< Apply everything (full fidelity replay)
  };
  ZYPP_DECLARE_FLAGS_AND_OPERATORS( ApplySetupFlags, ApplySetupFlag );

  class ZYPP_API_DEPTESTOMATIC TestcaseSetup
  {
  public:
    /// lock/keep node data extracted from trial nodes and applied as pool setup.
    using LockEntry = std::pair<std::string, std::map<std::string,std::string>>;

    TestcaseSetup();
    ~TestcaseSetup();

    Arch architecture () const;

    const std::optional<RepoData> &systemRepo() const;
    const std::vector<RepoData> &repos() const;

    // solver flags: default to false - set true if mentioned in <setup>
    ResolverFocus resolverFocus() const;

    const Pathname &globalPath() const;
    const Pathname &hardwareInfoFile() const;
    const Pathname &systemCheck() const;

    const target::Modalias::ModaliasList &modaliasList() const;
    const base::SetTracker<LocaleSet> &localesTracker() const;
    const std::vector<std::vector<std::string>> &vendorLists() const;
    const sat::StringQueue &autoinstalled() const;
    const std::set<std::string> &multiversionSpec() const;
    const std::vector<ForceInstall> &forceInstallTasks() const;
    const std::vector<LockEntry> &locks() const;

    bool set_licence() const;
    bool show_mediaid() const;

    bool ignorealreadyrecommended() const;
    bool onlyRequires() const;
    bool forceResolve() const;
    bool cleandepsOnRemove() const;
    bool noUpdateProvide() const;

    bool allowDowngrade() const;
    bool allowNameChange() const;
    bool allowArchChange() const;
    bool allowVendorChange() const;

    bool dupAllowDowngrade() const;
    bool dupAllowNameChange() const;
    bool dupAllowArchChange() const;
    bool dupAllowVendorChange() const;

    bool applySetup ( zypp::RepoManager &manager ) const;

    /** \overload Apply additional setup controlled by \a flags_r.
     *  Repos/arch/hardware are always applied regardless of flags.
     *  Pass \ref AS_ALL to apply everything (locales, solver flags,
     *  vendor lists, autoinstalled, modalias, multiversion).
     */
    bool applySetup ( zypp::RepoManager &manager, ApplySetupFlags flags_r ) const;

    static bool loadRepo (zypp::RepoManager &manager, const TestcaseSetup &setup, const RepoData &data );

    TestcaseSetupImpl &data();
    const TestcaseSetupImpl &data() const;

  private:
    RWCOW_pointer<TestcaseSetupImpl> _pimpl;
  };
}


#endif // ZYPP_MISC_TESTCASESETUPIMPL_H
