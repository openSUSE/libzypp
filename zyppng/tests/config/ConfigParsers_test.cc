#include <boost/test/unit_test.hpp>

#include <set>
#include <string>
#include <string_view>
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
