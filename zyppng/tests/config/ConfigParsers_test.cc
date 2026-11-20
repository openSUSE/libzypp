#include <boost/test/unit_test.hpp>

#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <zypp-core/Pathname.h>
#include <zypp-core/TriBool.h>

// GMF includes for types used directly in test bodies
#include <zypp/ng/config/zyppconfig.h>

import zyppng;

using namespace zyppng;
using namespace zyppng::config;

/**
 * Data-driven tests for the zypp.conf value parsers.
 *
 * The reference is legacy ZConfig/MediaConfig. Rows whose expectation deliberately
 * differs from legacy are marked "LEGACY-DIFF" — they are the checklist for a
 * future legacy-exact compat registration (ZConfig migration).
 */

namespace {

  template<typename T>
  struct Row {
    std::string_view input;
    T                expected;
  };

  /// true if \a v is a definite (non-indeterminate) TriBool equal to \a expected.
  bool triIs( const zypp::TriBool & v, bool expected )
  { return !zypp::indeterminate(v) && bool(v) == expected; }

} // namespace

BOOST_AUTO_TEST_SUITE(ConfigParsersTest)

// ── Bool ──────────────────────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(bool_true_tokens)
{
  // true tokens: 1 yes true always on + (case-insensitive) and any non-zero number
  for ( std::string_view in : { "1", "yes", "true", "always", "on", "+",
                                "YES", "On", "TRUE",
                                "2", "0x1", "-1",
                                "12abc" /* legacy strtonum prefix is non-zero */ } ) {
    BOOST_TEST_CONTEXT( "input='" << in << "'" ) {
      auto r = parseBool( in );
      BOOST_REQUIRE( r );
      BOOST_CHECK_EQUAL( *r, true );
    }
  }
}

BOOST_AUTO_TEST_CASE(bool_false_tokens)
{
  // false tokens: 0 no false never off - (case-insensitive)
  for ( std::string_view in : { "0", "no", "false", "never", "off", "-",
                                "NO", "Off", "FALSE" } ) {
    BOOST_TEST_CONTEXT( "input='" << in << "'" ) {
      auto r = parseBool( in );
      BOOST_REQUIRE( r );
      BOOST_CHECK_EQUAL( *r, false );
    }
  }
}

BOOST_AUTO_TEST_CASE(bool_unrecognised_is_error)
{
  // LEGACY-DIFF: legacy strToBool() silently keeps the current value; here the
  // key gets a ParseError and falls back to its default (same effective value).
  for ( std::string_view in : { "maybe", "", "yess" } ) {
    BOOST_TEST_CONTEXT( "input='" << in << "'" )
      BOOST_CHECK( !parseBool( in ) );
  }
}

// ── TriBool ───────────────────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(tribool_true_tokens)
{
  for ( std::string_view in : { "1", "yes", "true", "always", "on", "+", "YES", "2" } ) {
    BOOST_TEST_CONTEXT( "input='" << in << "'" ) {
      auto r = parseTriBool( in );
      BOOST_REQUIRE( r );
      BOOST_CHECK( triIs( *r, true ) );
    }
  }
}

BOOST_AUTO_TEST_CASE(tribool_false_tokens)
{
  // Regression: these used to come out as "indeterminate" because
  // strToBoolNodefault() returns false for a false token AND for garbage.
  for ( std::string_view in : { "0", "no", "false", "never", "off", "-", "NO", "Off" } ) {
    BOOST_TEST_CONTEXT( "input='" << in << "'" ) {
      auto r = parseTriBool( in );
      BOOST_REQUIRE( r );
      BOOST_CHECK( triIs( *r, false ) );
    }
  }
}

BOOST_AUTO_TEST_CASE(tribool_unrecognised_is_indeterminate_not_error)
{
  // "defer to the master gpgcheck switch" — exactly legacy strToTriBool().
  for ( std::string_view in : { "maybe", "", "auto" } ) {
    BOOST_TEST_CONTEXT( "input='" << in << "'" ) {
      auto r = parseTriBool( in );
      BOOST_REQUIRE( r );
      BOOST_CHECK( zypp::indeterminate( *r ) );
    }
  }
}

// ── Long / Unsigned ───────────────────────────────────────────────────────
// Reference: legacy str::strtonum = strtol/strtoul with base 0.

BOOST_AUTO_TEST_CASE(long_accepted_forms)
{
  const std::vector<Row<long>> rows {
    { "42",   42 },
    { "-5",   -5 },
    { "+3",   3 },
    { " 7",   7 },     // leading whitespace
    { "7 ",   7 },     // trailing whitespace
    { "0x10", 16 },    // hex
    { "010",  8 },     // octal (legacy base 0)
    { "0",    0 },
  };
  for ( const auto & row : rows ) {
    BOOST_TEST_CONTEXT( "input='" << row.input << "'" ) {
      auto r = parseLong( row.input );
      BOOST_REQUIRE( r );
      BOOST_CHECK_EQUAL( *r, row.expected );
    }
  }
}

BOOST_AUTO_TEST_CASE(long_rejected_forms)
{
  // LEGACY-DIFF (all rows): legacy never fails — it yields the numeric prefix or 0.
  // Here the key gets a ParseError and falls back to its default.
  const std::vector<std::pair<std::string_view, const char *>> rows {
    { "",                         "legacy 0" },
    { " ",                        "legacy 0" },
    { "abc",                      "legacy 0" },
    { "12abc",                    "legacy 12" },
    { "0x",                       "legacy 0" },
    { "08",                       "legacy 0 (invalid octal digit)" },
    { "99999999999999999999999",  "legacy LONG_MAX (saturated)" },
  };
  for ( const auto & [in, legacy] : rows ) {
    BOOST_TEST_CONTEXT( "input='" << in << "' (" << legacy << ")" )
      BOOST_CHECK( !parseLong( in ) );
  }
}

BOOST_AUTO_TEST_CASE(unsigned_accepted_forms)
{
  const std::vector<Row<unsigned>> rows {
    { "10",   10u },
    { "0x10", 16u },
    { " 5",   5u },
    { "+5",   5u },
    { "0",    0u },
  };
  for ( const auto & row : rows ) {
    BOOST_TEST_CONTEXT( "input='" << row.input << "'" ) {
      auto r = parseUnsigned( row.input );
      BOOST_REQUIRE( r );
      BOOST_CHECK_EQUAL( *r, row.expected );
    }
  }

  // The full unsigned range is accepted
  auto max = parseUnsigned( std::to_string( std::numeric_limits<unsigned>::max() ) );
  BOOST_REQUIRE( max );
  BOOST_CHECK_EQUAL( *max, std::numeric_limits<unsigned>::max() );
}

BOOST_AUTO_TEST_CASE(unsigned_rejected_forms)
{
  const std::vector<std::pair<std::string_view, const char *>> rows {
    { "-1",          "LEGACY-DIFF: legacy wraps around to UINT_MAX" },
    { "abc",         "LEGACY-DIFF: legacy 0" },
    { "",            "LEGACY-DIFF: legacy 0" },
    { "99999999999", "LEGACY-DIFF: legacy truncates" },   // > UINT_MAX
  };
  for ( const auto & [in, why] : rows ) {
    BOOST_TEST_CONTEXT( "input='" << in << "' (" << why << ")" )
      BOOST_CHECK( !parseUnsigned( in ) );
  }

  // One above UINT_MAX (only representable where unsigned long is wider)
  if constexpr ( sizeof(unsigned long) > sizeof(unsigned) ) {
    const unsigned long tooBig = static_cast<unsigned long>( std::numeric_limits<unsigned>::max() ) + 1ul;
    BOOST_CHECK( !parseUnsigned( std::to_string( tooBig ) ) );
  }
}

BOOST_AUTO_TEST_CASE(numbers_clamped_through_config)
{
  // The clamping parsers reuse parseLong, so they accept the same forms.
  ContextConfig cfg( zypp::Pathname("/") );
  registerZyppKeys( cfg );

  BOOST_REQUIRE( cfg.set( "media/transfer_timeout", "5000" ) );
  BOOST_CHECK_EQUAL( cfg.get(TransferTimeout), 3600L );       // clamped to [0, 3600]
  BOOST_REQUIRE( cfg.set( "media/transfer_timeout", "-5" ) );
  BOOST_CHECK_EQUAL( cfg.get(TransferTimeout), 0L );
  BOOST_REQUIRE( cfg.set( "media/transfer_timeout", "0x10" ) );
  BOOST_CHECK_EQUAL( cfg.get(TransferTimeout), 16L );

  BOOST_REQUIRE( cfg.set( "media/connect_timeout", "-5" ) );
  BOOST_CHECK_EQUAL( cfg.get(ConnectTimeout), 0L );           // clamped to [0, inf)
  BOOST_REQUIRE( cfg.set( "media/connect_timeout", "99999" ) );
  BOOST_CHECK_EQUAL( cfg.get(ConnectTimeout), 99999L );       // no upper bound
}

// ── String / StringSet / Path ─────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(string_is_identity)
{
  for ( std::string_view in : { "", " a b ", "x/y", "ünïcode" } ) {
    BOOST_TEST_CONTEXT( "input='" << in << "'" ) {
      auto r = parseString( in );
      BOOST_REQUIRE( r );
      BOOST_CHECK_EQUAL( *r, std::string(in) );
    }
  }
}

BOOST_AUTO_TEST_CASE(string_set)
{
  using Set = std::set<std::string>;
  const std::vector<Row<Set>> rows {
    { "a, b\tc",   { "a", "b", "c" } },
    { "a,a",       { "a" } },
    { "",          { } },
    { "a",         { "a" } },
    // splitEscaped: a backslash preserves the next character literally
    { "a\\,b",     { "a,b" } },
    // quoting is honoured too (legacy zypp.conf syntax)
    { "'a b' c",   { "a b", "c" } },
    { "\"x y\"",   { "x y" } },
    { "''",        { "" } },
  };
  for ( const auto & row : rows ) {
    BOOST_TEST_CONTEXT( "input='" << row.input << "'" ) {
      auto r = parseStringSet( row.input );
      BOOST_REQUIRE( r );
      BOOST_CHECK( *r == row.expected );
    }
  }
}

BOOST_AUTO_TEST_CASE(path_basic)
{
  // Escape/normalisation rules are covered in Config_test (path_key_rejects_escape).
  auto abs = parsePath( "/var/cache/zypp" );
  BOOST_REQUIRE( abs );
  BOOST_CHECK_EQUAL( *abs, zypp::Pathname("/var/cache/zypp") );

  // Pathname normalises "" to the empty path; it does not escape any root.
  auto empty = parsePath( "" );
  BOOST_REQUIRE( empty );
  BOOST_CHECK( empty->empty() );
}

// ── Arch ──────────────────────────────────────────────────────────────────
// Legacy: Arch(value) never fails; unknown names (and "") become custom archs.

BOOST_AUTO_TEST_CASE(arch_builtin)
{
  auto r = parseArch( "x86_64" );
  BOOST_REQUIRE( r );
  BOOST_CHECK( r->isBuiltIn() );
  BOOST_CHECK_EQUAL( r->asString(), "x86_64" );
}

BOOST_AUTO_TEST_CASE(arch_noarch)
{
  auto r = parseArch( "noarch" );
  BOOST_REQUIRE( r );
  BOOST_CHECK( *r == zypp::Arch_noarch );
}

BOOST_AUTO_TEST_CASE(arch_unknown_becomes_custom_arch)
{
  auto r = parseArch( "my_custom_arch" );
  BOOST_REQUIRE( r );
  BOOST_CHECK( !r->isBuiltIn() );
  BOOST_CHECK_EQUAL( r->asString(), "my_custom_arch" );
}

BOOST_AUTO_TEST_CASE(arch_empty_is_accepted_like_legacy)
{
  // Legacy builds Arch("") — an empty custom arch. NOT a LEGACY-DIFF.
  auto r = parseArch( "" );
  BOOST_REQUIRE( r );
  BOOST_CHECK( r->asString().empty() );
}

BOOST_AUTO_TEST_CASE(arch_roundtrip)
{
  for ( std::string_view in : { "x86_64", "noarch", "my_custom_arch", "" } ) {
    BOOST_TEST_CONTEXT( "input='" << in << "'" ) {
      auto first = parseArch( in );
      BOOST_REQUIRE( first );
      auto second = parseArch( serializeArch( *first ) );
      BOOST_REQUIRE( second );
      BOOST_CHECK( *second == *first );
    }
  }
}

// ── DownloadMode ──────────────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(download_mode)
{
  const std::vector<Row<zypp::DownloadMode>> rows {
    { "DownloadOnly",      zypp::DownloadOnly },
    { "downloadonly",      zypp::DownloadOnly },
    { "DownloadInAdvance", zypp::DownloadInAdvance },
    { "DownloadInHeaps",   zypp::DownloadInHeaps },
    { "DownloadAsNeeded",  zypp::DownloadAsNeeded },
    { "",                  zypp::DownloadDefault },   // "unset" sentinel
  };
  for ( const auto & row : rows ) {
    BOOST_TEST_CONTEXT( "input='" << row.input << "'" ) {
      auto r = parseDownloadMode( row.input );
      BOOST_REQUIRE( r );
      BOOST_CHECK( *r == row.expected );
    }
  }

  // LEGACY-DIFF: legacy deserializeDownloadMode() silently returns DownloadDefault
  BOOST_CHECK( !parseDownloadMode( "bogus" ) );
}

// ── ResolverFocus ─────────────────────────────────────────────────────────

BOOST_AUTO_TEST_CASE(resolver_focus)
{
  const std::vector<Row<zypp::ResolverFocus>> rows {
    { "Default",   zypp::ResolverFocus::Default },
    { "job",       zypp::ResolverFocus::Job },
    { "Installed", zypp::ResolverFocus::Installed },
    { "UPDATE",    zypp::ResolverFocus::Update },
    { "",          zypp::ResolverFocus::Default },
  };
  for ( const auto & row : rows ) {
    BOOST_TEST_CONTEXT( "input='" << row.input << "'" ) {
      auto r = parseResolverFocus( row.input );
      BOOST_REQUIRE( r );
      BOOST_CHECK( *r == row.expected );
    }
  }

  // LEGACY-DIFF: legacy fromString() returns false and keeps the default
  BOOST_CHECK( !parseResolverFocus( "bogus" ) );
}

// ── Round trip: parse(serialize(v)) == v ──────────────────────────────────

BOOST_AUTO_TEST_CASE(roundtrip_core_types)
{
  for ( bool b : { true, false } ) {
    auto r = parseBool( serializeBool( b ) );
    BOOST_REQUIRE( r );
    BOOST_CHECK_EQUAL( *r, b );
  }

  for ( bool b : { true, false } ) {
    auto r = parseTriBool( serializeTriBool( zypp::TriBool(b) ) );
    BOOST_REQUIRE( r );
    BOOST_CHECK( triIs( *r, b ) );
  }
  {
    // indeterminate serializes to "" and must come back as indeterminate
    auto r = parseTriBool( serializeTriBool( zypp::TriBool(zypp::indeterminate) ) );
    BOOST_REQUIRE( r );
    BOOST_CHECK( zypp::indeterminate( *r ) );
  }

  {
    auto r = parseString( serializeString( " a b " ) );
    BOOST_REQUIRE( r );
    BOOST_CHECK_EQUAL( *r, " a b " );
  }

  {
    // numbers: parse(serialize(v)) == v, including the extremes
    for ( long v : { 0L, -5L, 42L, std::numeric_limits<long>::max(), std::numeric_limits<long>::min() } ) {
      BOOST_TEST_CONTEXT( "long=" << v ) {
        auto r = parseLong( serializeLong( v ) );
        BOOST_REQUIRE( r );
        BOOST_CHECK_EQUAL( *r, v );
      }
    }
    for ( unsigned v : { 0u, 42u, std::numeric_limits<unsigned>::max() } ) {
      BOOST_TEST_CONTEXT( "unsigned=" << v ) {
        auto r = parseUnsigned( serializeUnsigned( v ) );
        BOOST_REQUIRE( r );
        BOOST_CHECK_EQUAL( *r, v );
      }
    }
  }

  {
    // Must round-trip for ANY set: the host serializes it for media workers.
    const std::vector<std::set<std::string>> sets {
      { },
      { "a", "b", "c" },
      { "download.opensuse.org" },
      { "a,b", "c d", "e\tf" },                  // separators inside elements
      { "it's", "say \"hi\"", "back\\slash" },   // quote characters and backslash
      { "", "x" },                               // empty element
    };
    for ( const auto & in : sets ) {
      const std::string text = serializeStringSet( in );
      BOOST_TEST_CONTEXT( "serialized='" << text << "'" ) {
        auto r = parseStringSet( text );
        BOOST_REQUIRE( r );
        BOOST_CHECK( *r == in );
      }
    }

    // Plain values stay readable / unchanged
    BOOST_CHECK_EQUAL( serializeStringSet( { "a", "b" } ), "a, b" );
    BOOST_CHECK_EQUAL( serializeStringSet( { "a,b" } ),   "a\\,b" );
  }

  for ( const char * p : { "/var/cache/zypp", "some/rel/path", "" } ) {
    BOOST_TEST_CONTEXT( "path='" << p << "'" ) {
      auto first = parsePath( p );
      BOOST_REQUIRE( first );
      auto second = parsePath( serializePath( *first ) );
      BOOST_REQUIRE( second );
      BOOST_CHECK_EQUAL( *second, *first );
    }
  }
}

BOOST_AUTO_TEST_CASE(string_set_survives_worker_transport)
{
  // Host serializes every key for the media workers; the worker only knows the
  // media keys (GeoipHostnames is one of them).
  ContextConfig host( zypp::Pathname("/") );
  registerZyppKeys( host );
  host.set( GeoipHostnames, std::set<std::string>{ "mirror one.example", "a,b", "plain.example" } );

  ContextConfig worker( zypp::Pathname("/") );
  registerMediaKeys( worker );
  auto errs = worker.parseEntries( host.serializeEntries() );

  BOOST_CHECK( errs.empty() );
  BOOST_CHECK( worker.get( GeoipHostnames )
               == ( std::set<std::string>{ "mirror one.example", "a,b", "plain.example" } ) );
}

BOOST_AUTO_TEST_CASE(roundtrip_app_types)
{
  for ( zypp::DownloadMode m : { zypp::DownloadDefault, zypp::DownloadOnly,
                                 zypp::DownloadInAdvance, zypp::DownloadInHeaps,
                                 zypp::DownloadAsNeeded } ) {
    auto r = parseDownloadMode( serializeDownloadMode( m ) );
    BOOST_REQUIRE( r );
    BOOST_CHECK( *r == m );
  }

  for ( zypp::ResolverFocus f : { zypp::ResolverFocus::Default, zypp::ResolverFocus::Job,
                                  zypp::ResolverFocus::Installed, zypp::ResolverFocus::Update } ) {
    auto r = parseResolverFocus( serializeResolverFocus( f ) );
    BOOST_REQUIRE( r );
    BOOST_CHECK( *r == f );
  }
}

BOOST_AUTO_TEST_SUITE_END()
