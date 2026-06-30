/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/ng/config/zyppconfig.h
 *
 * Domain key token definitions for the zypp configuration engine.
 *
 * This header is C++17 compatible and may be included by both the legacy
 * zypp/ tier and the zyppng/ module tier.
 *
 * All Key<T> and PathKey tokens are declared as inline constexpr globals in
 * namespace zypp::config.  The free function registerZyppKeys() registers
 * all tokens into a Config instance — call it once before the first parse().
 */
#ifndef ZYPP_NG_CONFIG_ZYPPCONFIG_H
#define ZYPP_NG_CONFIG_ZYPPCONFIG_H

#include <filesystem>
#include <set>
#include <string>

#include <zypp-core/APIConfig.h>
#include <zypp-core/TriBool.h>
#include <zypp-core/ng/config/config.h>
#include <zypp/Arch.h>
#include <zypp/DownloadMode.h>
#include <zypp/Locale.h>
#include <zypp/ResolverFocus.h>
#include <zypp-core/ng/pipelines/expected.h>

namespace zyppng::config {

  // ---------------------------------------------------------------------------
  // Parse helpers — declared here, implemented in zyppconfig.cc
  // Return expected<T> — on failure the error holds a std::exception_ptr.
  // ---------------------------------------------------------------------------

  expected<bool>                      parseBool          ( std::string_view s );
  expected<long>                      parseLong          ( std::string_view s );
  expected<unsigned>                  parseUnsigned      ( std::string_view s );
  expected<std::string>               parseString        ( std::string_view s );
  expected<zypp::Arch>                parseArch          ( std::string_view s );
  expected<zypp::Locale>              parseLocale        ( std::string_view s );
  expected<zypp::LocaleSet>           parseLocaleSet     ( std::string_view s );  ///< splits on ", \t"
  expected<zypp::TriBool>             parseTriBool       ( std::string_view s );
  expected<zypp::ResolverFocus>       parseResolverFocus ( std::string_view s );
  expected<zypp::DownloadMode>        parseDownloadMode  ( std::string_view s );
  expected<std::set<std::string>>     parseStringSet     ( std::string_view s );  ///< splits on ", \t"
  /**
   * Parse a filesystem path from a config value.
   * Rejects paths that start with ".." after lexically_normal() — such paths
   * could escape a chroot root when composed with root().
   * Absolute paths and paths like "some/../dir" (normalises to "dir") are accepted.
   */
  expected<std::filesystem::path>     parsePath          ( std::string_view s );

  // ---------------------------------------------------------------------------
  // Serialize helpers — declared here, implemented in zyppconfig.cc
  // Must round-trip with their corresponding parse helpers.
  // ---------------------------------------------------------------------------

  std::string serializeBool       ( const bool & v );
  std::string serializeLong       ( const long & v );
  std::string serializeUnsigned   ( const unsigned & v );
  std::string serializeString     ( const std::string & v );
  std::string serializePath       ( const std::filesystem::path & v );
  std::string serializeArch       ( const zypp::Arch & v );
  std::string serializeLocale     ( const zypp::Locale & v );
  std::string serializeLocaleSet  ( const zypp::LocaleSet & v );
  std::string serializeTriBool    ( const zypp::TriBool & v );
  std::string serializeResolverFocus( const zypp::ResolverFocus & v );
  std::string serializeDownloadMode ( const zypp::DownloadMode & v );
  std::string serializeStringSet  ( const std::set<std::string> & v );

  // ---------------------------------------------------------------------------
  // Default helpers — declared here, implemented in zyppconfig.cc
  // ---------------------------------------------------------------------------

  /** Autodetect system locale from LC_ALL → LC_MESSAGES → LANG env vars. */
  zypp::Locale defaultTextLocale( const Config & );

  // ---------------------------------------------------------------------------
  // Key tokens — canonical name "section/keyname"
  // ---------------------------------------------------------------------------

  // ── Architecture ─────────────────────────────────────────────────────────
  inline constexpr Key<zypp::Arch> SystemArchitecture {
    "main/arch", "arch",
    [](const Config &) { return zypp::Arch::detectSystemArchitecture(); },
    parseArch, serializeArch };

  // ── Locales ───────────────────────────────────────────────────────────────
  inline constexpr Key<zypp::Locale> TextLocale {
    "main/textLocale", "",
    defaultTextLocale, parseLocale, serializeLocale };
  inline constexpr Key<zypp::LocaleSet> RepoRefreshLocales {
    "main/repo.refresh.locales", "repo.refresh.locales",
    [](const Config &) { return zypp::LocaleSet{}; },
    parseLocaleSet, serializeLocaleSet };

  // ── Paths — stored as configured; callers compose root() / path themselves.
  // parsePath() rejects paths starting with ".." after lexically_normal().
  //
  // Cross-key dependencies mirror legacy ZConfig behaviour:
  //   metadatadir / solvfilesdir / packagesdir default to cachedir / subdir
  //   reposdir / servicesdir / varsdir / vendordir / locksfile /
  //   solver.checkSystemFile[Dir] / needreboot[File/Path] / multiversiondir
  //   all default to configdir / subdir
  //   If the dependent key is explicitly set, the default is not consulted.

  inline constexpr Key<std::filesystem::path> ConfigPath {
    "main/configdir", "configdir",
    [](const Config&) { return std::filesystem::path("/etc/zypp"); },
    parsePath, serializePath };
  inline constexpr Key<std::filesystem::path> RepoCachePath {
    "main/cachedir", "cachedir",
    [](const Config&) { return std::filesystem::path("/var/cache/zypp"); },
    parsePath, serializePath };

  // cachedir-relative defaults
  inline constexpr Key<std::filesystem::path> RepoMetadataPath {
    "main/metadatadir", "metadatadir",
    [](const Config& c) { return c.get(RepoCachePath) / "raw"; },
    parsePath, serializePath };
  inline constexpr Key<std::filesystem::path> RepoSolvfilesPath {
    "main/solvfilesdir", "solvfilesdir",
    [](const Config& c) { return c.get(RepoCachePath) / "solv"; },
    parsePath, serializePath };
  inline constexpr Key<std::filesystem::path> RepoPackagesPath {
    "main/packagesdir", "packagesdir",
    [](const Config& c) { return c.get(RepoCachePath) / "packages"; },
    parsePath, serializePath };

  // configdir-relative defaults
  inline constexpr Key<std::filesystem::path> KnownReposPath {
    "main/reposdir", "reposdir",
    [](const Config& c) { return c.get(ConfigPath) / "repos.d"; },
    parsePath, serializePath };
  inline constexpr Key<std::filesystem::path> KnownServicesPath {
    "main/servicesdir", "servicesdir",
    [](const Config& c) { return c.get(ConfigPath) / "services.d"; },
    parsePath, serializePath };
  inline constexpr Key<std::filesystem::path> VarsPath {
    "main/varsdir", "varsdir",
    [](const Config& c) { return c.get(ConfigPath) / "vars.d"; },
    parsePath, serializePath };
  inline constexpr Key<std::filesystem::path> VendorPath {
    "main/vendordir", "vendordir",
    [](const Config& c) { return c.get(ConfigPath) / "vendors.d"; },
    parsePath, serializePath };
  inline constexpr Key<std::filesystem::path> LocksFile {
    "main/locksfile", "locksfile.path",
    [](const Config& c) { return c.get(ConfigPath) / "locks"; },
    parsePath, serializePath };
  inline constexpr Key<std::filesystem::path> HistoryLogFile {
    "main/history.logfile", "history.logfile",
    [](const Config&) { return std::filesystem::path("/var/log/zypp/history"); },
    parsePath, serializePath };

  // Credentials
  inline constexpr Key<std::filesystem::path> CredentialsDir {
    "media/credentials.dir", "credentials.global.dir",
    [](const Config&) { return std::filesystem::path("/etc/zypp/credentials.d"); },
    parsePath, serializePath };
  inline constexpr Key<std::filesystem::path> CredentialsFile {
    "media/credentials.file", "credentials.global.file",
    [](const Config&) { return std::filesystem::path("/etc/zypp/credentials.cat"); },
    parsePath, serializePath };

  // ── Geo IP behaviour ──────────────────────────────────────────────────────
  inline constexpr Key<bool> GeoipEnabled {
    "media/geoip.enabled", "download.use_geoip_mirror",
    [](const Config&) { return true; },
    parseBool, serializeBool };

  // Geoip cache — derived from repoCachePath in legacy
  inline constexpr Key<std::filesystem::path> GeoipCachePath {
    "media/geoip.cachedir", "",
    [](const Config& c) { return c.get(RepoCachePath) / "geoip.d"; },
    parsePath, serializePath };

  // ── Repository behaviour ──────────────────────────────────────────────────
  inline constexpr Key<bool> RepoAddProbe {
    "repo/add.probe", "repo.add.probe",
    [](const Config&) { return false; },
    parseBool, serializeBool };

  inline constexpr Key<unsigned> RepoRefreshDelay {
    "repo/refresh.delay", "repo.refresh.delay",
    [](const Config&) { return 10u; },
    parseUnsigned, serializeUnsigned };

  // ── Download transport (mapped to "media/" section) ───────────────────────
  inline constexpr Key<long> MaxConcurrentConnections {
    "media/max_concurrent_connections", "download.max_concurrent_connections",
    [](const Config&) { return 5L; },
    parseLong, serializeLong };

  inline constexpr Key<long> MinDownloadSpeed {
    "media/min_download_speed", "download.min_download_speed",
    [](const Config&) { return 0L; },
    parseLong, serializeLong };

  inline constexpr Key<long> MaxDownloadSpeed {
    "media/max_download_speed", "download.max_download_speed",
    [](const Config&) { return 0L; },
    parseLong, serializeLong };

  inline constexpr Key<long> MaxSilentTries {
    "media/max_silent_tries", "download.max_silent_tries",
    [](const Config&) { return 1L; },
    parseLong, serializeLong };

  inline constexpr Key<long> TransferTimeout {
    "media/transfer_timeout", "download.transfer_timeout",
    [](const Config&) { return 180L; },
    // clamped [0, 3600] — matches legacy ZConfig/MediaConfig behaviour
    [](std::string_view s) -> expected<long> {
      const auto& expV = parseLong(s);
      if (!expV) return expV;
      long v = *expV;
      if (v < 0)    v = 0;
      if (v > 3600) v = 3600;
      return v;
    },
    serializeLong };

  inline constexpr Key<long> ConnectTimeout {
    "media/connect_timeout", "download.connect_timeout",
    [](const Config&) { return 60L; },
    // clamped [0, ∞) — negative values silently become 0
    [](std::string_view s) -> expected<long> {
      const auto& expV = parseLong(s);
      if (!expV) return expV;
      return std::max(0L, *expV);
    },
    serializeLong };

  // Host-relative (not root-relative) — the download machinery mounts
  // physical media on the running host, not inside the managed root.
  inline constexpr Key<std::filesystem::path> MediaMountDir {
    "media/media_mountdir", "download.media_mountdir",
    [](const Config&) { return std::filesystem::path("/var/adm/mount"); },
    [](std::string_view s) -> expected<std::filesystem::path> {
      return std::filesystem::path(s);
    },
    serializePath };

  inline constexpr Key<bool> UseDeltarpm {
    "media/use_deltarpm", "download.use_deltarpm",
    [](const Config&) { return bool(LIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT); },
    parseBool, serializeBool };

  inline constexpr Key<bool> UseDeltarpmAlways {
    "media/use_deltarpm_always", "download.use_deltarpm.always",
    [](const Config&) { return false; },
    parseBool, serializeBool };

  inline constexpr Key<bool> MediaPreferDownload {
    "media/prefer_download", "download.media_preference",
    [](const Config&) { return true; },
    // parse: any value != "volatile" → true
    [](std::string_view s) -> expected<bool> {
      return (zypp::str::compareCI(std::string(s), "volatile") != 0);
    },
    // serialize: emit "download" or "volatile" to match the zypp.conf convention
    [](const bool & v) -> std::string { return v ? "download" : "volatile"; } };

  // ── GPG / Signature ───────────────────────────────────────────────────────
  inline constexpr Key<bool> GpgCheck {
    "main/gpgcheck", "gpgcheck",
    [](const Config&) { return true; },
    parseBool, serializeBool };

  inline constexpr Key<zypp::TriBool> RepoGpgCheck {
    "main/repo_gpgcheck", "repo_gpgcheck",
    [](const Config&) { return zypp::TriBool{zypp::indeterminate}; },
    parseTriBool, serializeTriBool };

  inline constexpr Key<zypp::TriBool> PkgGpgCheck {
    "main/pkg_gpgcheck", "pkg_gpgcheck",
    [](const Config&) { return zypp::TriBool{zypp::indeterminate}; },
    parseTriBool, serializeTriBool };

  // ── Solver ────────────────────────────────────────────────────────────────
  inline constexpr Key<zypp::ResolverFocus> SolverFocus {
    "solver/focus", "solver.focus",
    [](const Config&) { return zypp::ResolverFocus::Default; },
    parseResolverFocus, serializeResolverFocus };

  inline constexpr Key<bool> SolverOnlyRequires {
    "solver/onlyRequires", "solver.onlyRequires",
    [](const Config&) { return false; },
    parseBool, serializeBool };

  inline constexpr Key<bool> SolverAllowVendorChange {
    "solver/allowVendorChange", "solver.allowVendorChange",
    [](const Config&) { return false; },
    parseBool, serializeBool };

  inline constexpr Key<bool> SolverCleandepsOnRemove {
    "solver/cleandepsOnRemove", "solver.cleandepsOnRemove",
    [](const Config&) { return false; },
    parseBool, serializeBool };

  inline constexpr Key<bool> SolverDupAllowDowngrade {
    "solver/dupAllowDowngrade", "solver.dupAllowDowngrade",
    [](const Config&) { return true; },
    parseBool, serializeBool };

  inline constexpr Key<bool> SolverDupAllowNameChange {
    "solver/dupAllowNameChange", "solver.dupAllowNameChange",
    [](const Config&) { return true; },
    parseBool, serializeBool };

  inline constexpr Key<bool> SolverDupAllowArchChange {
    "solver/dupAllowArchChange", "solver.dupAllowArchChange",
    [](const Config&) { return true; },
    parseBool, serializeBool };

  inline constexpr Key<bool> SolverDupAllowVendorChange {
    "solver/dupAllowVendorChange", "solver.dupAllowVendorChange",
    [](const Config&) { return false; },
    parseBool, serializeBool };

  inline constexpr Key<unsigned> SolverUpgradeTestcasesToKeep {
    "solver/upgradeTestcasesToKeep", "solver.upgradeTestcasesToKeep",
    [](const Config&) { return 2u; },
    parseUnsigned, serializeUnsigned };

  inline constexpr Key<bool> SolverUpgradeRemoveDroppedPackages {
    "solver/upgradeRemoveDroppedPackages", "solver.upgradeRemoveDroppedPackages",
    [](const Config&) { return true; },
    parseBool, serializeBool };

  // ── Multiversion ──────────────────────────────────────────────────────────
  inline constexpr Key<std::set<std::string>> MultiversionSpec {
    "main/multiversion", "multiversion",
    [](const Config&) { return std::set<std::string>{}; },
    parseStringSet, serializeStringSet };

  inline constexpr Key<std::string> MultiversionKernels {
    "main/multiversion.kernels", "multiversion.kernels",
    [](const Config&) { return std::string{}; },
    parseString, serializeString };

  // ── RPM install flags ─────────────────────────────────────────────────────
  inline constexpr Key<bool> RpmExcludeDocs {
    "main/rpm.install.excludedocs", "rpm.install.excludedocs",
    [](const Config&) { return false; },
    parseBool, serializeBool };

  // ── Miscellaneous ─────────────────────────────────────────────────────────
  inline constexpr Key<long> LockTimeout {
    "main/lock_timeout", "lock_timeout",
    [](const Config&) { return 0L; },
    parseLong, serializeLong };

  inline constexpr Key<bool> ApplyLocksFile {
    "main/locksfile.apply", "locksfile.apply",
    [](const Config&) { return true; },
    parseBool, serializeBool };

  inline constexpr Key<std::string> UserData {
    "main/userData", "",
    [](const Config&) { return std::string{}; },
    parseString, serializeString };

  // ── Plugins — host-relative (plugins run on the host, not in the target)
  inline constexpr Key<std::filesystem::path> PluginsPath {
    "main/pluginsPath", "",
    [](const Config&) { return std::filesystem::path("/usr/lib/zypp/plugins"); },
    parsePath, serializePath };

  // ── Update paths — hardcoded constants in legacy (parsed but ignored)
  inline constexpr Key<std::filesystem::path> UpdateDataPath {
    "main/update.datadir", "update.datadir",
    [](const Config&) { return std::filesystem::path("/var/adm"); },
    parsePath, serializePath };
  inline constexpr Key<std::filesystem::path> UpdateScriptsPath {
    "main/update.scriptsdir", "update.scriptsdir",
    [](const Config&) { return std::filesystem::path("/var/adm/update-scripts"); },
    parsePath, serializePath };
  inline constexpr Key<std::filesystem::path> UpdateMessagesPath {
    "main/update.messagesdir", "update.messagesdir",
    [](const Config&) { return std::filesystem::path("/var/adm/update-messages"); },
    parsePath, serializePath };

  inline constexpr Key<std::string> UpdateMessagesNotify {
    "main/update.messages.notify", "update.messages.notify",
    [](const Config&) { return std::string{}; },
    parseString, serializeString };

  // ── Solver check system — configdir-relative ─────────────────────────────
  inline constexpr Key<std::filesystem::path> SolverCheckSystemFile {
    "main/solver.checkSystemFile", "solver.checkSystemFile",
    [](const Config& c) { return c.get(ConfigPath) / "systemCheck"; },
    parsePath, serializePath };
  inline constexpr Key<std::filesystem::path> SolverCheckSystemFileDir {
    "main/solver.checkSystemFileDir", "solver.checkSystemFileDir",
    [](const Config& c) { return c.get(ConfigPath) / "systemCheck.d"; },
    parsePath, serializePath };

  // ── Needreboot — configdir-relative ──────────────────────────────────────
  inline constexpr Key<std::filesystem::path> NeedrebootFile {
    "main/needreboot.file", "",
    [](const Config& c) { return c.get(ConfigPath) / "needreboot"; },
    parsePath, serializePath };
  inline constexpr Key<std::filesystem::path> NeedrebootPath {
    "main/needreboot.dir", "",
    [](const Config& c) { return c.get(ConfigPath) / "needreboot.d"; },
    parsePath, serializePath };

  // ── Multiversion directory — configdir-relative ───────────────────────────
  inline constexpr Key<std::filesystem::path> MultiversionPath {
    "main/multiversiondir", "multiversiondir",
    [](const Config& c) { return c.get(ConfigPath) / "multiversion.d"; },
    parsePath, serializePath };

  // ── Distroverpkg ──────────────────────────────────────────────────────────
  inline constexpr Key<std::string> Distroverpkg {
    "main/distroverpkg", "",
    [](const Config&) { return std::string{"system-release"}; },
    parseString, serializeString };

  // ── Geoip hostnames ───────────────────────────────────────────────────────
  inline constexpr Key<std::set<std::string>> GeoipHostnames {
    "main/geoip.hosts", "",
    [](const Config&) {
      std::set<std::string> hosts;
      hosts.insert("download.opensuse.org");
      return hosts;
    },
    parseStringSet, serializeStringSet };

  // ── Repository label preference ───────────────────────────────────────────
  inline constexpr Key<bool> RepoLabelIsAlias {
    "main/repo.label.isAlias", "",
    [](const Config&) { return false; },
    parseBool, serializeBool };

  inline constexpr Key<zypp::DownloadMode> CommitDownloadMode {
    "main/commit.downloadMode", "commit.downloadMode",
    [](const Config&) { return zypp::DownloadDefault; },
    parseDownloadMode, serializeDownloadMode };

  // ---------------------------------------------------------------------------
  // Computed accessors — cross-key semantics matching legacy ZConfig
  // ---------------------------------------------------------------------------

  /**
   * Effective use-deltarpm-always value, mirroring ZConfig::download_use_deltarpm_always():
   *   return get(UseDeltarpm) && get(UseDeltarpmAlways);
   *
   * Consumers MUST call this function rather than cfg.get(UseDeltarpmAlways) directly
   * so that UseDeltarpm acts as a master switch, matching legacy behaviour.
   */
  inline bool effectiveUseDeltarpmAlways( const Config & cfg )
  {
    return cfg.get(UseDeltarpm) && cfg.get(UseDeltarpmAlways);
  }

  // ---------------------------------------------------------------------------
  // Registration
  // ---------------------------------------------------------------------------

  /**
   * Register all zypp domain key tokens into \a cfg.
   * Call once before cfg.parse() — keys not in the registry are stored as raw
   * strings and never type-converted. Must be called before parse().
   */
  void registerZyppKeys( Config& cfg );

} // namespace zyppng::config

#endif // ZYPP_NG_CONFIG_ZYPPCONFIG_H
