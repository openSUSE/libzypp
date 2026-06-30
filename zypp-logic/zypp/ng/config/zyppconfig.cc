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

  expected<bool> parseBool( std::string_view s )
  {
    try {
      // strToTriBool returns indeterminate for unrecognised values —
      // unambiguous unlike strToBoolNodefault which returns false for both
      // "parsed as false" and "could not parse".
      zypp::TriBool result = zypp::str::strToTriBool( std::string(s).c_str() );
      if ( zypp::indeterminate(result) )
        throw std::invalid_argument( "not a boolean value: '" + std::string(s) + "'" );
      return bool(result);
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<long> parseLong( std::string_view s )
  {
    try {
      std::size_t pos = 0;
      long val = std::stol( std::string(s), &pos );
      if ( pos != s.size() )
        throw std::invalid_argument( "trailing characters in long value: '" + std::string(s) + "'" );
      return val;
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<unsigned> parseUnsigned( std::string_view s )
  {
    try {
      std::size_t pos = 0;
      unsigned long val = std::stoul( std::string(s), &pos );
      if ( pos != s.size() )
        throw std::invalid_argument( "trailing characters in unsigned value: '" + std::string(s) + "'" );
      if ( val > std::numeric_limits<unsigned>::max() )
        throw std::out_of_range( "value out of range for unsigned: '" + std::string(s) + "'" );
      return static_cast<unsigned>(val);
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<std::string> parseString( std::string_view s )
  {
    return std::string(s);
  }

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

  expected<TriBool> parseTriBool( std::string_view s )
  {
    try {
      bool val = false;
      if ( zypp::str::strToBoolNodefault( std::string(s).c_str(), val ) )
        return TriBool(val);
      // indeterminate is not an error — it is a valid config value meaning
      // "defer to the master gpgcheck switch".
      return TriBool(zypp::indeterminate);
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

  expected<std::set<std::string>> parseStringSet( std::string_view s )
  {
    try {
      std::vector<std::string> tokens;
      zypp::str::splitEscaped( std::string(s).c_str(), std::back_inserter(tokens), ", \t" );
      return std::set<std::string>( tokens.begin(), tokens.end() );
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<std::filesystem::path> parsePath( std::string_view s )
  {
    try {
      std::filesystem::path p = std::filesystem::path(s).lexically_normal();
      // A normalised path that still starts with ".." escapes the root.
      // e.g. "../etc" → unsafe; "some/../etc" → normalises to "etc" → safe.
      if ( !p.empty() && *p.begin() == ".." )
        throw std::invalid_argument(
          "path would escape root after normalisation: '" + std::string(s) + "'" );
      return p;
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  // ---------------------------------------------------------------------------
  // Serialize helpers
  // ---------------------------------------------------------------------------

  std::string serializeBool( const bool & v )
  { return v ? "true" : "false"; }

  std::string serializeLong( const long & v )
  { return std::to_string(v); }

  std::string serializeUnsigned( const unsigned & v )
  { return std::to_string(v); }

  std::string serializeString( const std::string & v )
  { return v; }

  std::string serializePath( const std::filesystem::path & v )
  { return v.string(); }

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

  std::string serializeTriBool( const zypp::TriBool & v )
  {
    if ( zypp::indeterminate(v) ) return "";
    return bool(v) ? "true" : "false";
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

  std::string serializeStringSet( const std::set<std::string> & v )
  {
    std::string result;
    for ( const auto & s : v ) {
      if ( !result.empty() ) result += ", ";
      result += s;
    }
    return result;
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
