/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zyppng/lib/zypp/ng/contextconfig.cppm
 *
 * Module partition zyppng:contextconfig.
 *
 * Pulls the domain key tokens from zypp-logic/zypp/ng/config/zyppconfig.h
 * into the C++20 module system and re-exports them under zyppng::config so
 * that zyppng consumers use: import zyppng; ... cfg.get(zyppng::config::GpgCheck)
 *
 * ContextConfig is a type alias for zyppng::config::Config — the domain key
 * tokens in zyppng::config give it its identity.  No separate wrapper class
 * is needed.
 *
 * IMPORTANT: registerZyppKeys() is NOT called automatically. Every construction
 * path must explicitly call registerZyppKeys(cfg) before cfg.parse(). Use one
 * of the makeSystemConfig() / makeRootConfig() / makeDefaultConfig() factories,
 * or follow the same explicit sequence in the Context constructor.
 */
module;
// Global module fragment — #include is only permitted here.
#include <zypp/ng/config/zyppconfig.h>

export module zyppng:contextconfig;

export namespace zyppng {
    /// ContextConfig is Config — owned exclusively by Context.
    using ContextConfig = ::zyppng::config::Config;
}

export namespace zyppng::config {

    // ── ParseError ────────────────────────────────────────────────────────
    using ::zyppng::config::ParseError;

    // ── Parse helpers ─────────────────────────────────────────────────────
    using ::zyppng::config::parseBool;
    using ::zyppng::config::parseLong;
    using ::zyppng::config::parseUnsigned;
    using ::zyppng::config::parseString;
    using ::zyppng::config::parseArch;
    using ::zyppng::config::parseLocale;
    using ::zyppng::config::parseLocaleSet;
    using ::zyppng::config::parseTriBool;
    using ::zyppng::config::parseResolverFocus;
    using ::zyppng::config::parseDownloadMode;
    using ::zyppng::config::parseStringSet;

    // ── Key tokens ────────────────────────────────────────────────────────
    using ::zyppng::config::SystemArchitecture;
    using ::zyppng::config::TextLocale;
    using ::zyppng::config::RepoRefreshLocales;
    using ::zyppng::config::ConfigPath;
    using ::zyppng::config::RepoCachePath;
    using ::zyppng::config::RepoMetadataPath;
    using ::zyppng::config::RepoSolvfilesPath;
    using ::zyppng::config::RepoPackagesPath;
    using ::zyppng::config::KnownReposPath;
    using ::zyppng::config::KnownServicesPath;
    using ::zyppng::config::VarsPath;
    using ::zyppng::config::VendorPath;
    using ::zyppng::config::LocksFile;
    using ::zyppng::config::HistoryLogFile;
    using ::zyppng::config::CredentialsDir;
    using ::zyppng::config::CredentialsFile;
    using ::zyppng::config::RepoAddProbe;
    using ::zyppng::config::RepoRefreshDelay;
    using ::zyppng::config::GeoipEnabled;
    using ::zyppng::config::MaxConcurrentConnections;
    using ::zyppng::config::MinDownloadSpeed;
    using ::zyppng::config::MaxDownloadSpeed;
    using ::zyppng::config::MaxSilentTries;
    using ::zyppng::config::TransferTimeout;
    using ::zyppng::config::UseDeltarpm;
    using ::zyppng::config::UseDeltarpmAlways;
    using ::zyppng::config::MediaPreferDownload;
    using ::zyppng::config::GpgCheck;
    using ::zyppng::config::RepoGpgCheck;
    using ::zyppng::config::PkgGpgCheck;
    using ::zyppng::config::SolverFocus;
    using ::zyppng::config::SolverOnlyRequires;
    using ::zyppng::config::SolverAllowVendorChange;
    using ::zyppng::config::SolverCleandepsOnRemove;
    using ::zyppng::config::SolverDupAllowDowngrade;
    using ::zyppng::config::SolverDupAllowNameChange;
    using ::zyppng::config::SolverDupAllowArchChange;
    using ::zyppng::config::SolverDupAllowVendorChange;
    using ::zyppng::config::SolverUpgradeTestcasesToKeep;
    using ::zyppng::config::SolverUpgradeRemoveDroppedPackages;
    using ::zyppng::config::MultiversionSpec;
    using ::zyppng::config::MultiversionKernels;
    using ::zyppng::config::RpmExcludeDocs;
    using ::zyppng::config::LockTimeout;
    using ::zyppng::config::ApplyLocksFile;
    using ::zyppng::config::UserData;
    using ::zyppng::config::CommitDownloadMode;

    // ── Plugins ───────────────────────────────────────────────────────────
    using ::zyppng::config::PluginsPath;

    // ── Update paths ──────────────────────────────────────────────────────
    using ::zyppng::config::UpdateDataPath;
    using ::zyppng::config::UpdateScriptsPath;
    using ::zyppng::config::UpdateMessagesPath;
    using ::zyppng::config::UpdateMessagesNotify;

    // ── Solver check system ───────────────────────────────────────────────
    using ::zyppng::config::SolverCheckSystemFile;
    using ::zyppng::config::SolverCheckSystemFileDir;

    // ── Needreboot ────────────────────────────────────────────────────────
    using ::zyppng::config::NeedrebootFile;
    using ::zyppng::config::NeedrebootPath;

    // ── Multiversion directory ────────────────────────────────────────────
    using ::zyppng::config::MultiversionPath;

    // ── Geoip ─────────────────────────────────────────────────────────────
    using ::zyppng::config::GeoipCachePath;
    using ::zyppng::config::GeoipHostnames;

    // ── Distro / repo label ───────────────────────────────────────────────
    using ::zyppng::config::Distroverpkg;
    using ::zyppng::config::RepoLabelIsAlias;

    // ── Computed accessors ────────────────────────────────────────────────
    using ::zyppng::config::effectiveUseDeltarpmAlways;

} // export namespace zyppng::config

// ---------------------------------------------------------------------------
// ContextConfig factories — exported
// ---------------------------------------------------------------------------
// These are the canonical construction paths. Each one:
//   1. Constructs Config(root)
//   2. Calls registerZyppKeys(cfg) to populate the registry
//   3. Calls cfg.parse() and returns any ParseErrors
//
// Use makeDefaultConfig() for tests / synthetic contexts (no file I/O).

export namespace zyppng {

std::pair<ContextConfig, std::vector<config::ParseError>> makeSystemConfig();

std::pair<ContextConfig, std::vector<config::ParseError>>
makeRootConfig( std::filesystem::path root );

ContextConfig makeDefaultConfig();

} // export namespace zyppng
