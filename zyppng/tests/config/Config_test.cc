#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <fstream>
#include <string>

// GMF includes for types used directly in test bodies
#include <zypp/ng/config/zyppconfig.h>
#include <zypp-core/fs/TmpPath.h>
#include <zypp-core/fs/PathInfo.h>

import zyppng;

using namespace zyppng;
using namespace zyppng::config;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Write a minimal zypp.conf under root_r/etc/zypp/zypp.conf.
 * Creates the directory hierarchy as needed.
 */
static void writeConf( const zypp::Pathname & root, const std::string & content )
{
    zypp::Pathname confDir = root / "etc/zypp";
    ::zypp::filesystem::assert_dir( confDir );
    zypp::Pathname confFile = confDir / "zypp.conf";
    std::ofstream f( confFile.c_str() );
    BOOST_REQUIRE_MESSAGE( f.is_open(), "Failed to open " << confFile );
    f << content;
}

/** Construct a Config with zypp keys registered but no parse (pure defaults). */
static ContextConfig makeDefaults()
{
    ContextConfig cfg( std::filesystem::path("/") );
    registerZyppKeys( cfg );
    return cfg;
}

/** Construct a Config for root, register keys and parse. */
static std::pair<ContextConfig, std::vector<ParseError>>
makeConfig( std::filesystem::path root )
{
    ContextConfig cfg( std::move(root) );
    registerZyppKeys( cfg );
    auto errors = cfg.parse();
    return { std::move(cfg), std::move(errors) };
}

// ---------------------------------------------------------------------------
// Test suite
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(ConfigTest)

// ── Defaults ─────────────────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(defaults_arch)
{
    auto cfg = makeDefaults();
    // Default arch is autodetected — just verify it is non-empty
    BOOST_CHECK( !cfg.get(SystemArchitecture).empty() );
}

BOOST_AUTO_TEST_CASE(defaults_gpgcheck)
{
    auto cfg = makeDefaults();
    BOOST_CHECK_EQUAL( cfg.get(GpgCheck), true );
}

BOOST_AUTO_TEST_CASE(defaults_transfer_timeout)
{
    auto cfg = makeDefaults();
    BOOST_CHECK_EQUAL( cfg.get(TransferTimeout), 180L );
}

BOOST_AUTO_TEST_CASE(defaults_solver_focus)
{
    auto cfg = makeDefaults();
    BOOST_CHECK( cfg.get(SolverFocus) == zypp::ResolverFocus::Default );
}

// ── Typed key set / get / reset ───────────────────────────────────────────

BOOST_AUTO_TEST_CASE(set_get_reset_typed)
{
    auto cfg = makeDefaults();

    // Override
    cfg.set( GpgCheck, false );
    BOOST_CHECK_EQUAL( cfg.get(GpgCheck), false );

    // Reset reverts to default
    cfg.reset( GpgCheck );
    BOOST_CHECK_EQUAL( cfg.get(GpgCheck), true );
}

BOOST_AUTO_TEST_CASE(set_get_reset_string_keyed)
{
    auto cfg = makeDefaults();

    cfg.set<bool>( GpgCheck.name, false );
    BOOST_CHECK_EQUAL( cfg.get<bool>(GpgCheck.name), false );

    cfg.reset( GpgCheck.name );
    BOOST_CHECK_EQUAL( cfg.get<bool>(GpgCheck.name), true );
}

BOOST_AUTO_TEST_CASE(override_does_not_affect_file_value)
{
    zypp::filesystem::TmpDir root;
    writeConf( root.path(), "[main]\ngpgcheck = false\n" );

    auto [cfg, errs] = makeConfig( root.path().asString() );
    BOOST_REQUIRE( errs.empty() );

    // File says false
    BOOST_CHECK_EQUAL( cfg.get(GpgCheck), false );

    // Override to true
    cfg.set( GpgCheck, true );
    BOOST_CHECK_EQUAL( cfg.get(GpgCheck), true );

    // Reset clears override — falls back to file value (false)
    cfg.reset( GpgCheck );
    BOOST_CHECK_EQUAL( cfg.get(GpgCheck), false );
}

// ── Path key ─────────────────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(path_key_root_relative_default)
{
    auto cfg = makeDefaults();
    // Default RepoCachePath is the configured absolute path — NOT root-relative
    BOOST_CHECK_EQUAL( cfg.get(RepoCachePath),
                       std::filesystem::path("/var/cache/zypp") );
}

BOOST_AUTO_TEST_CASE(path_key_override_and_reset)
{
    ContextConfig cfg( std::filesystem::path("/mnt") );
    registerZyppKeys( cfg );

    cfg.set( RepoCachePath, std::filesystem::path("/custom/cache") );
    BOOST_CHECK_EQUAL( cfg.get(RepoCachePath),
                       std::filesystem::path("/custom/cache") );

    cfg.reset( RepoCachePath );
    BOOST_CHECK_EQUAL( cfg.get(RepoCachePath),
                       std::filesystem::path("/var/cache/zypp") );
}

BOOST_AUTO_TEST_CASE(path_key_rejects_escape)
{
    // parsePath must reject paths that escape root via leading ".."
    auto r1 = parsePath("../escape");
    BOOST_CHECK( !r1 );

    auto r2 = parsePath("some/../../escape");
    BOOST_CHECK( !r2 );

    // Internal ".." that resolves safely is accepted
    auto r3 = parsePath("some/path/../etc");
    BOOST_REQUIRE( r3 );
    BOOST_CHECK_EQUAL( *r3, std::filesystem::path("some/etc") );

    // Absolute paths are always safe
    auto r4 = parsePath("/absolute/../path");
    BOOST_REQUIRE( r4 );
    BOOST_CHECK_EQUAL( *r4, std::filesystem::path("/path") );
}

// ── Parse — valid config ──────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(parse_reads_gpgcheck)
{
    zypp::filesystem::TmpDir root;
    writeConf( root.path(), "[main]\ngpgcheck = false\n" );

    auto [cfg, errs] = makeConfig( root.path().asString() );
    BOOST_CHECK( errs.empty() );
    BOOST_CHECK_EQUAL( cfg.get(GpgCheck), false );
}

BOOST_AUTO_TEST_CASE(parse_reads_transfer_timeout)
{
    zypp::filesystem::TmpDir root;
    writeConf( root.path(), "[main]\ndownload.transfer_timeout = 60\n" );

    auto [cfg, errs] = makeConfig( root.path().asString() );
    BOOST_CHECK( errs.empty() );
    // legacy alias maps to media/transfer_timeout
    BOOST_CHECK_EQUAL( cfg.get(TransferTimeout), 60L );
}

BOOST_AUTO_TEST_CASE(parse_clamps_transfer_timeout)
{
    zypp::filesystem::TmpDir root;
    writeConf( root.path(), "[main]\ndownload.transfer_timeout = 9999\n" );

    auto [cfg, errs] = makeConfig( root.path().asString() );
    BOOST_CHECK( errs.empty() );
    BOOST_CHECK_EQUAL( cfg.get(TransferTimeout), 3600L );
}

BOOST_AUTO_TEST_CASE(parse_reads_solver_focus)
{
    zypp::filesystem::TmpDir root;
    writeConf( root.path(), "[main]\nsolver.focus = Job\n" );

    auto [cfg, errs] = makeConfig( root.path().asString() );
    BOOST_CHECK( errs.empty() );
    BOOST_CHECK( cfg.get(SolverFocus) == zypp::ResolverFocus::Job );
}

// ── Parse — error handling ────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(parse_error_bad_bool)
{
    zypp::filesystem::TmpDir root;
    writeConf( root.path(), "[main]\ngpgcheck = notabool\n" );

    auto [cfg, errs] = makeConfig( root.path().asString() );

    // Must have exactly one parse error
    BOOST_REQUIRE_EQUAL( errs.size(), 1u );
    BOOST_CHECK_EQUAL( errs[0].key,      "main/gpgcheck" );
    BOOST_CHECK_EQUAL( errs[0].rawValue, "notabool" );
    BOOST_CHECK( errs[0].error != nullptr );

    // Key falls through to compiled-in default (true)
    BOOST_CHECK_EQUAL( cfg.get(GpgCheck), true );
}

BOOST_AUTO_TEST_CASE(parse_error_bad_long)
{
    zypp::filesystem::TmpDir root;
    writeConf( root.path(), "[main]\nlock_timeout = notanumber\n" );

    auto [cfg, errs] = makeConfig( root.path().asString() );

    BOOST_REQUIRE_EQUAL( errs.size(), 1u );
    BOOST_CHECK_EQUAL( errs[0].key, LockTimeout.name );
    // Falls through to default (0)
    BOOST_CHECK_EQUAL( cfg.get(LockTimeout), 0L );
}

BOOST_AUTO_TEST_CASE(parse_error_bad_resolver_focus)
{
    zypp::filesystem::TmpDir root;
    writeConf( root.path(), "[main]\nsolver.focus = InvalidFocus\n" );

    auto [cfg, errs] = makeConfig( root.path().asString() );

    BOOST_REQUIRE_EQUAL( errs.size(), 1u );
    BOOST_CHECK_EQUAL( errs[0].key, SolverFocus.name );
    BOOST_CHECK( cfg.get(SolverFocus) == zypp::ResolverFocus::Default );
}

BOOST_AUTO_TEST_CASE(parse_error_exception_is_rethrowable)
{
    zypp::filesystem::TmpDir root;
    writeConf( root.path(), "[main]\ngpgcheck = notabool\n" );

    auto [cfg, errs] = makeConfig( root.path().asString() );
    BOOST_REQUIRE( !errs.empty() );

    // The exception_ptr must be rethrowable and hold a std::exception
    BOOST_CHECK_NO_THROW(
        try { std::rethrow_exception( errs[0].error ); }
        catch ( const std::exception & ) {}
    );
}

// ── Cross-key default ─────────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(use_deltarpm_defaults)
{
    auto cfg = makeDefaults();

    // UseDeltarpm default is determined by LIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT.
    BOOST_CHECK_EQUAL( cfg.get(UseDeltarpm),       bool(LIBZYPP_CONFIG_USE_DELTARPM_BY_DEFAULT) );
    // UseDeltarpmAlways raw default is always false.
    BOOST_CHECK_EQUAL( cfg.get(UseDeltarpmAlways), false );
}

BOOST_AUTO_TEST_CASE(use_deltarpm_always_masked_by_deltarpm)
{
    auto cfg = makeDefaults();

    // With UseDeltarpm=true and UseDeltarpmAlways=true, effective is true.
    cfg.set( UseDeltarpm,       true );
    cfg.set( UseDeltarpmAlways, true );
    BOOST_CHECK_EQUAL( effectiveUseDeltarpmAlways(cfg), true );

    // Masking: UseDeltarpm=false suppresses UseDeltarpmAlways regardless of its stored value.
    cfg.set( UseDeltarpm, false );
    BOOST_CHECK_EQUAL( cfg.get(UseDeltarpmAlways),     true  ); // raw stored value unchanged
    BOOST_CHECK_EQUAL( effectiveUseDeltarpmAlways(cfg), false ); // masked by UseDeltarpm

    // Re-enable UseDeltarpm — effective is true again.
    cfg.set( UseDeltarpm, true );
    BOOST_CHECK_EQUAL( effectiveUseDeltarpmAlways(cfg), true );

    // Reset UseDeltarpmAlways — raw reverts to default (false) → effective is false.
    cfg.reset( UseDeltarpmAlways );
    BOOST_CHECK_EQUAL( cfg.get(UseDeltarpmAlways),     false );
    BOOST_CHECK_EQUAL( effectiveUseDeltarpmAlways(cfg), false );
}

BOOST_AUTO_TEST_CASE(get_baseline_ignores_override)
{
    zypp::filesystem::TmpDir root;
    writeConf( root.path(), "[main]\ncachedir = /file/cache\n" );

    auto [cfg, errs] = makeConfig( root.path().asString() );
    BOOST_REQUIRE( errs.empty() );

    // Before override: get() and getBaseline() agree
    BOOST_CHECK_EQUAL( cfg.get(RepoCachePath),      std::filesystem::path("/file/cache") );
    BOOST_CHECK_EQUAL( cfg.getBaseline(RepoCachePath), std::filesystem::path("/file/cache") );

    // After override: get() returns override, getBaseline() returns file value
    cfg.set( RepoCachePath, std::filesystem::path("/runtime/cache") );
    BOOST_CHECK_EQUAL( cfg.get(RepoCachePath),         std::filesystem::path("/runtime/cache") );
    BOOST_CHECK_EQUAL( cfg.getBaseline(RepoCachePath), std::filesystem::path("/file/cache") );

    // After reset: both agree again on file value
    cfg.reset( RepoCachePath );
    BOOST_CHECK_EQUAL( cfg.get(RepoCachePath),         std::filesystem::path("/file/cache") );
    BOOST_CHECK_EQUAL( cfg.getBaseline(RepoCachePath), std::filesystem::path("/file/cache") );
}

BOOST_AUTO_TEST_CASE(get_baseline_falls_through_to_default)
{
    // No config file — getBaseline() must return defaultFn result
    auto cfg = makeDefaults();
    BOOST_CHECK_EQUAL( cfg.getBaseline(RepoCachePath),
                       std::filesystem::path("/var/cache/zypp") );

    // Override does not affect baseline
    cfg.set( RepoCachePath, std::filesystem::path("/runtime/cache") );
    BOOST_CHECK_EQUAL( cfg.getBaseline(RepoCachePath),
                       std::filesystem::path("/var/cache/zypp") );
}

// ── Cross-key path dependencies ───────────────────────────────────────────

BOOST_AUTO_TEST_CASE(cross_key_path_cachedir)
{
    // metadatadir / solvfilesdir / packagesdir default relative to cachedir
    auto cfg = makeDefaults();
    BOOST_CHECK_EQUAL( cfg.get(RepoMetadataPath),  cfg.get(RepoCachePath) / "raw"      );
    BOOST_CHECK_EQUAL( cfg.get(RepoSolvfilesPath), cfg.get(RepoCachePath) / "solv"     );
    BOOST_CHECK_EQUAL( cfg.get(RepoPackagesPath),  cfg.get(RepoCachePath) / "packages" );

    // Override cachedir — derived paths must follow
    cfg.set( RepoCachePath, std::filesystem::path("/custom/cache") );
    BOOST_CHECK_EQUAL( cfg.get(RepoMetadataPath),  std::filesystem::path("/custom/cache/raw")      );
    BOOST_CHECK_EQUAL( cfg.get(RepoSolvfilesPath), std::filesystem::path("/custom/cache/solv")     );
    BOOST_CHECK_EQUAL( cfg.get(RepoPackagesPath),  std::filesystem::path("/custom/cache/packages") );

    // Explicitly set metadatadir — must NOT follow cachedir override
    cfg.set( RepoMetadataPath, std::filesystem::path("/explicit/meta") );
    BOOST_CHECK_EQUAL( cfg.get(RepoMetadataPath), std::filesystem::path("/explicit/meta") );
}

BOOST_AUTO_TEST_CASE(cross_key_path_configdir)
{
    // reposdir / servicesdir etc. default relative to configdir
    auto cfg = makeDefaults();
    BOOST_CHECK_EQUAL( cfg.get(KnownReposPath),    cfg.get(ConfigPath) / "repos.d"        );
    BOOST_CHECK_EQUAL( cfg.get(KnownServicesPath), cfg.get(ConfigPath) / "services.d"     );
    BOOST_CHECK_EQUAL( cfg.get(MultiversionPath),  cfg.get(ConfigPath) / "multiversion.d" );

    // Override configdir — derived paths must follow
    cfg.set( ConfigPath, std::filesystem::path("/custom/etc/zypp") );
    BOOST_CHECK_EQUAL( cfg.get(KnownReposPath),   std::filesystem::path("/custom/etc/zypp/repos.d")       );
    BOOST_CHECK_EQUAL( cfg.get(MultiversionPath), std::filesystem::path("/custom/etc/zypp/multiversion.d") );
}

// ── Change signal ─────────────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(signal_fires_on_set)
{
    auto cfg = makeDefaults();

    bool slotCalled = false;
    std::any receivedValue;

    cfg.sigChanged("main/gpgcheck").connect(
        [&]( const std::any & v ) {
            slotCalled    = true;
            receivedValue = v;
        });

    cfg.set( GpgCheck, false );

    BOOST_CHECK( slotCalled );
    BOOST_REQUIRE( receivedValue.has_value() );
    BOOST_CHECK_EQUAL( std::any_cast<bool>(receivedValue), false );
}

BOOST_AUTO_TEST_CASE(signal_fires_on_reset)
{
    auto cfg = makeDefaults();

    cfg.set( GpgCheck, false );

    bool slotCalled = false;
    std::any receivedValue;
    cfg.sigChanged("main/gpgcheck").connect(
        [&]( const std::any & v ) {
            slotCalled    = true;
            receivedValue = v;
        });

    cfg.reset( GpgCheck );

    BOOST_CHECK( slotCalled );
    // After reset the effective value is the default (true)
    BOOST_REQUIRE( receivedValue.has_value() );
    BOOST_CHECK_EQUAL( std::any_cast<bool>(receivedValue), true );
}

BOOST_AUTO_TEST_CASE(signal_not_allocated_until_requested)
{
    auto cfg = makeDefaults();
    // Setting a value must not crash even if no signal was ever requested
    BOOST_CHECK_NO_THROW( cfg.set( GpgCheck, false ) );
}

// ── Raw / preview key ─────────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(raw_key_roundtrip)
{
    auto cfg = makeDefaults();

    BOOST_CHECK( !cfg.getRaw("preview/single_rpmtrans").has_value() );

    cfg.setRaw( "preview/single_rpmtrans", "true" );
    auto val = cfg.getRaw("preview/single_rpmtrans");

    BOOST_REQUIRE( val.has_value() );
    BOOST_CHECK_EQUAL( *val, "true" );
}

BOOST_AUTO_TEST_CASE(unknown_key_stored_raw_during_parse)
{
    zypp::filesystem::TmpDir root;
    writeConf( root.path(), "[main]\nmy.custom.key = hello\n" );

    auto [cfg, errs] = makeConfig( root.path().asString() );
    BOOST_CHECK( errs.empty() );  // unknown keys are not errors

    auto val = cfg.getRaw("main/my.custom.key");
    BOOST_REQUIRE( val.has_value() );
    BOOST_CHECK_EQUAL( *val, "hello" );
}

// ── Legacy alias ──────────────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(legacy_alias_resolved_on_parse)
{
    zypp::filesystem::TmpDir root;
    // Legacy key name in zypp.conf — must map to canonical "media/transfer_timeout"
    writeConf( root.path(), "[main]\ndownload.transfer_timeout = 90\n" );

    auto [cfg, errs] = makeConfig( root.path().asString() );
    BOOST_CHECK( errs.empty() );
    BOOST_CHECK_EQUAL( cfg.get(TransferTimeout), 90L );
    // Canonical key must also be accessible by string
    BOOST_CHECK_EQUAL( cfg.get<long>(TransferTimeout.name), 90L );
}

// ── forEach ───────────────────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(for_each_visits_set_and_file_keys)
{
    zypp::filesystem::TmpDir root;
    writeConf( root.path(), "[main]\ngpgcheck = false\n" );

    auto [cfg, errs] = makeConfig( root.path().asString() );
    cfg.set( LockTimeout, 30L );  // programmatic override

    std::set<std::string> visited;
    cfg.forEach([&]( std::string_view key, const auto & ) {
        visited.insert( std::string(key) );
    });

    // File-parsed key must be present
    BOOST_CHECK( visited.count(std::string(GpgCheck.name)) );
    // Override key must be present
    BOOST_CHECK( visited.count(std::string(LockTimeout.name)) );
}

BOOST_AUTO_TEST_CASE(for_each_skips_default_only_keys)
{
    auto cfg = makeDefaults();
    // No parse, no set — nothing should appear in forEach

    std::size_t count = 0;
    cfg.forEach([&]( std::string_view, const auto & ) { ++count; });
    BOOST_CHECK_EQUAL( count, 0u );
}

// ── Serialization ─────────────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(serialize_roundtrip)
{
    // Set a mix of values then serialize and parse back — all values must match.
    auto cfg = makeDefaults();
    cfg.set( GpgCheck,        false );
    cfg.set( TransferTimeout, 60L   );
    cfg.set( RepoCachePath,   std::filesystem::path("/custom/cache") );
    cfg.setRaw( "preview/single_rpmtrans", "true" );

    std::string blob = cfg.serialize();
    BOOST_REQUIRE( !blob.empty() );

    // Parse into a fresh Config
    ContextConfig cfg2( std::filesystem::path("/") );
    registerZyppKeys( cfg2 );
    auto errors = cfg2.parseFromString(blob);
    BOOST_CHECK( errors.empty() );

    BOOST_CHECK_EQUAL( cfg2.get(GpgCheck),        false );
    BOOST_CHECK_EQUAL( cfg2.get(TransferTimeout),  60L   );
    BOOST_CHECK_EQUAL( cfg2.get(RepoCachePath),    std::filesystem::path("/custom/cache") );
    auto raw = cfg2.getRaw("preview/single_rpmtrans");
    BOOST_REQUIRE( raw.has_value() );
    BOOST_CHECK_EQUAL( *raw, "true" );
}

BOOST_AUTO_TEST_CASE(serialize_sections_not_duplicated)
{
    // Keys from the same section must appear under one [section] block.
    auto cfg = makeDefaults();
    std::string blob = cfg.serialize();

    // Count occurrences of "[main]" — must be exactly 1
    std::size_t count = 0;
    std::size_t pos   = 0;
    while ((pos = blob.find("[main]", pos)) != std::string::npos) {
        ++count;
        ++pos;
    }
    BOOST_CHECK_EQUAL( count, 1u );
}

BOOST_AUTO_TEST_CASE(serialize_defaults_included)
{
    // serialize() emits all effective values including defaults —
    // a worker process with no zypp.conf must still get the full picture.
    auto cfg = makeDefaults();
    std::string blob = cfg.serialize();

    // A few spot-checks that default values are present
    BOOST_CHECK( blob.find("transfer_timeout = 180") != std::string::npos );
    BOOST_CHECK( blob.find("gpgcheck = true")        != std::string::npos );
}

BOOST_AUTO_TEST_CASE(serialize_parseFromString_errors)
{
    // A bad value in a serialized blob must produce a ParseError, not a crash.
    auto cfg = makeDefaults();
    std::string blob = cfg.serialize();

    // Inject a bad value by appending to the [main] section
    blob += "\n[main]\ngpgcheck = notabool\n";

    ContextConfig cfg2( std::filesystem::path("/") );
    registerZyppKeys( cfg2 );
    auto errors = cfg2.parseFromString(blob);

    // Must have at least one error for the bad gpgcheck value
    BOOST_CHECK( !errors.empty() );
    bool found = false;
    for (const auto & e : errors)
        if (e.key == "main/gpgcheck") found = true;
    BOOST_CHECK( found );
}

BOOST_AUTO_TEST_SUITE_END()
