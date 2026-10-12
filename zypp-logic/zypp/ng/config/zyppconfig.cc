/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include <zypp/ng/config/zyppconfig.h>

#include <cstdlib>    // getenv
#include <cstring>    // strcmp
#include <sstream>
#include <stdexcept>
#include <vector>

#include <zypp-core/base/Logger.h>
#include <zypp-core/base/String.h>

namespace zyppng::config {

  using zypp::Arch;
  using zypp::Locale;
  using zypp::LocaleSet;
  using zypp::TriBool;
  using zypp::ResolverFocus;
  using zypp::DownloadMode;

  // ---------------------------------------------------------------------------
  // Parse helpers
  // ---------------------------------------------------------------------------

  expected<Arch> parseArch( std::string_view s )
  {
    try {
      Arch a( (std::string(s)) );
      if ( a == zypp::Arch_noarch && std::string(s) != "noarch" )
        throw std::invalid_argument( "unknown architecture: '" + std::string(s) + "'" );
      return a;
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<Locale> parseLocale( std::string_view s )
  {
    try {
      Locale loc( (std::string(s)) );
      if ( loc == Locale::noCode )
        throw std::invalid_argument( "unknown locale: '" + std::string(s) + "'" );
      return loc;
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<LocaleSet> parseLocaleSet( std::string_view s )
  {
    try {
      std::vector<std::string> tokens;
      zypp::str::splitEscaped( std::string(s).c_str(), std::back_inserter(tokens), ", \t" );
      LocaleSet result;
      for ( const auto & t : tokens )
        if ( !t.empty() )
          result.insert( zypp::Locale(t) );
      return result;
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<ResolverFocus> parseResolverFocus( std::string_view s )
  {
    try {
      ResolverFocus result { ResolverFocus::Default };
      if ( !fromString( std::string(s), result ) )
        throw std::invalid_argument( "unknown ResolverFocus value: '" + std::string(s) + "'" );
      return result;
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<DownloadMode> parseDownloadMode( std::string_view s )
  {
    try {
      // Empty string → DownloadDefault (the "unset" sentinel).
      // Matches legacy ZConfig behaviour and allows round-trip with
      // serializeDownloadMode which emits "" for DownloadDefault.
      zypp::DownloadMode result { zypp::DownloadDefault };
      if ( s.empty() )
        return result;
      if ( !zypp::deserialize( std::string(s), result ) )
        throw std::invalid_argument( "unknown DownloadMode value: '" + std::string(s) + "'" );
      return result;
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  // ---------------------------------------------------------------------------
  // Serialize helpers
  // ---------------------------------------------------------------------------

  std::string serializeArch( const zypp::Arch & v )
  { return v.asString(); }

  std::string serializeLocale( const zypp::Locale & v )
  { return v.asString(); }

  std::string serializeLocaleSet( const zypp::LocaleSet & v )
  {
    std::string result;
    for ( const auto & locale : v ) {
      if ( !result.empty() ) result += ", ";
      result += locale.asString();
    }
    return result;
  }

  std::string serializeResolverFocus( const zypp::ResolverFocus & v )
  { return zypp::asString(v); }

  std::string serializeDownloadMode( const zypp::DownloadMode & v )
  {
    // DownloadDefault is the "unset" sentinel — serialize as empty string so
    // parseDownloadMode (which returns DownloadDefault on unrecognised input)
    // round-trips correctly. All other values serialize via operator<<.
    if ( v == zypp::DownloadDefault )
      return {};
    std::ostringstream oss;
    oss << v;
    return oss.str();
  }

  // ---------------------------------------------------------------------------
  // Default helpers
  // ---------------------------------------------------------------------------

  Locale defaultTextLocale( const Config & )
  {
    // Autodetect from environment — mirrors ZConfig::_autodetectTextLocale().
    // Scans LC_ALL → LC_MESSAGES → LANG; falls back to Locale::enCode.
    // Not cached: re-reading on each call keeps test isolation correct when
    // setenv() is called between Context constructions.
    const char * envlist[] = { "LC_ALL", "LC_MESSAGES", "LANG", nullptr };
    for ( const char ** e = envlist; *e; ++e ) {
      const char * val = ::getenv( *e );
      if ( val && *val
           && ::strcmp(val, "POSIX") != 0
           && ::strcmp(val, "C")     != 0 )
      {
        Locale loc( val );
        if ( loc != Locale::noCode )
          return loc;
      }
    }
    return Locale::enCode;
  }

  // ---------------------------------------------------------------------------
  // registerZyppKeys
  // ---------------------------------------------------------------------------

  void registerZyppKeys( Config & cfg )
  {
    // ── Architecture ───────────────────────────────────────────────────────
    cfg.registerKey( SystemArchitecture );

    // ── Locales ────────────────────────────────────────────────────────────
    cfg.registerKey( TextLocale );
    cfg.registerKey( RepoRefreshLocales );

    // ── Paths ──────────────────────────────────────────────────────────────
    cfg.registerKey( ConfigPath );
    cfg.registerKey( RepoCachePath );
    cfg.registerKey( RepoMetadataPath );
    cfg.registerKey( RepoSolvfilesPath );
    cfg.registerKey( RepoPackagesPath );
    cfg.registerKey( KnownReposPath );
    cfg.registerKey( KnownServicesPath );
    cfg.registerKey( VarsPath );
    cfg.registerKey( VendorPath );
    cfg.registerKey( LocksFile );
    cfg.registerKey( HistoryLogFile );
    cfg.registerKey( CredentialsDir );
    cfg.registerKey( CredentialsFile );

    // ── Repository behaviour ───────────────────────────────────────────────
    cfg.registerKey( RepoAddProbe );
    cfg.registerKey( RepoRefreshDelay );
    cfg.registerKey( GeoipEnabled );

    // ── Download transport ─────────────────────────────────────────────────
    cfg.registerKey( MaxConcurrentConnections );
    cfg.registerKey( MinDownloadSpeed );
    cfg.registerKey( MaxDownloadSpeed );
    cfg.registerKey( MaxSilentTries );
    cfg.registerKey( TransferTimeout );
    cfg.registerKey( ConnectTimeout );
    cfg.registerKey( MediaMountDir );
    cfg.registerKey( UseDeltarpm );
    cfg.registerKey( UseDeltarpmAlways );
    cfg.registerKey( MediaPreferDownload );

    // ── GPG / Signature ────────────────────────────────────────────────────
    cfg.registerKey( GpgCheck );
    cfg.registerKey( RepoGpgCheck );
    cfg.registerKey( PkgGpgCheck );

    // ── Solver ─────────────────────────────────────────────────────────────
    cfg.registerKey( SolverFocus );
    cfg.registerKey( SolverOnlyRequires );
    cfg.registerKey( SolverAllowVendorChange );
    cfg.registerKey( SolverCleandepsOnRemove );
    cfg.registerKey( SolverDupAllowDowngrade );
    cfg.registerKey( SolverDupAllowNameChange );
    cfg.registerKey( SolverDupAllowArchChange );
    cfg.registerKey( SolverDupAllowVendorChange );
    cfg.registerKey( SolverUpgradeTestcasesToKeep );
    cfg.registerKey( SolverUpgradeRemoveDroppedPackages );

    // ── Multiversion ───────────────────────────────────────────────────────
    cfg.registerKey( MultiversionSpec );
    cfg.registerKey( MultiversionKernels );

    // ── RPM install flags ──────────────────────────────────────────────────
    cfg.registerKey( RpmExcludeDocs );

    // ── Miscellaneous ──────────────────────────────────────────────────────
    cfg.registerKey( LockTimeout );
    cfg.registerKey( ApplyLocksFile );
    cfg.registerKey( UserData );
    cfg.registerKey( CommitDownloadMode );

    // ── Plugins ────────────────────────────────────────────────────────────
    cfg.registerKey( PluginsPath );

    // ── Update paths ───────────────────────────────────────────────────────
    cfg.registerKey( UpdateDataPath );
    cfg.registerKey( UpdateScriptsPath );
    cfg.registerKey( UpdateMessagesPath );
    cfg.registerKey( UpdateMessagesNotify );

    // ── Solver check system ────────────────────────────────────────────────
    cfg.registerKey( SolverCheckSystemFile );
    cfg.registerKey( SolverCheckSystemFileDir );

    // ── Needreboot ─────────────────────────────────────────────────────────
    cfg.registerKey( NeedrebootFile );
    cfg.registerKey( NeedrebootPath );

    // ── Multiversion directory ─────────────────────────────────────────────
    cfg.registerKey( MultiversionPath );

    // ── Geoip ──────────────────────────────────────────────────────────────
    cfg.registerKey( GeoipCachePath );
    cfg.registerKey( GeoipHostnames );

    // ── Distro / repo label ────────────────────────────────────────────────
    cfg.registerKey( Distroverpkg );
    cfg.registerKey( RepoLabelIsAlias );
  }

} // namespace zyppng::config
