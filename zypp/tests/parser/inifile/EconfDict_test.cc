#include <iostream>
#include <boost/test/unit_test.hpp>
#include <zypp-core/base/LogTools.h>
#include <zypp-core/parser/EconfDict>

using std::cout;
using std::endl;
using namespace zypp;
using EconfDict      = zypp::parser::EconfDict;
using EconfException = zypp::parser::EconfException;
using namespace boost::unit_test;

#define DATADIR ( Pathname(TESTS_SRC_DIR) / "/parser/inifile/data/EconfDict" )

BOOST_AUTO_TEST_CASE(initialize)
{
  // No matter how ZYPPCONFDIR is configured, the testcases use "/usr/etc"
  EconfDict::defaultDistconfDir( "/usr/etc" );
}

BOOST_AUTO_TEST_CASE(stem_test)
{
  // config_r must denote a config file stem below some root dir:
  // [PROJECT/]EXAMPLE[.SUFFIX]
  BOOST_REQUIRE_THROW   ( EconfDict( "", DATADIR / "empty" ), EconfException ); // stem must not be empty
  BOOST_REQUIRE_THROW   ( EconfDict( "/", DATADIR / "empty" ), EconfException ); // stem must not be empty
  BOOST_REQUIRE_THROW   ( EconfDict( "./", DATADIR / "empty" ), EconfException ); // stem must not be empty
  BOOST_REQUIRE_THROW   ( EconfDict( "../", DATADIR / "empty" ), EconfException ); // stem must not refer to '../'
  BOOST_REQUIRE_THROW   ( EconfDict( "../bar", DATADIR / "empty" ), EconfException ); // stem must not refer to '../'
  // EXAMPLE
  BOOST_REQUIRE_NO_THROW( EconfDict( "bar", DATADIR / "empty" ) );
  BOOST_REQUIRE_NO_THROW( EconfDict( "/bar", DATADIR / "empty" ) );
  BOOST_REQUIRE_NO_THROW( EconfDict( "./bar", DATADIR / "empty" ) );
  // PROJECT/EXAMPLE
  BOOST_REQUIRE_NO_THROW( EconfDict( "foo/bar", DATADIR / "empty" ) );
  BOOST_REQUIRE_NO_THROW( EconfDict( "/foo/bar", DATADIR / "empty" ) );
  BOOST_REQUIRE_NO_THROW( EconfDict( "./foo/bar", DATADIR / "empty" ) );
  // PROJECT/EXAMPLE.SUFFIX
  BOOST_REQUIRE_NO_THROW( EconfDict( "foo/bar.conf", DATADIR / "empty" ) );
  BOOST_REQUIRE_NO_THROW( EconfDict( "/foo/bar.conf", DATADIR / "empty" ) );
  BOOST_REQUIRE_NO_THROW( EconfDict( "./foo/bar.conf", DATADIR / "empty" ) );
}

BOOST_AUTO_TEST_CASE(empty)
{
  // An empty tree. Some config paths exist, but as directories.
  // We want no errors and no data.
  EconfDict a( "foo/bar.conf", DATADIR / "empty" );
  std::string got = str::sprint( dump(a) );
  BOOST_CHECK_EQUAL( got, "" );
}

// The basic full layout for CONFD (/etc /run /usr/etc).
//
// foo/bar.conf:a = a /CONFD/foo/bar.conf
// foo/bar.conf:b = b /CONFD/foo/bar.conf
// foo/bar.conf:c = c /CONFD/foo/bar.conf
// foo/bar.conf:d = d /CONFD/foo/bar.conf
//
// foo/bar.conf.d/a.conf:a = a /CONFD/foo/bar.conf.d/a.conf
// foo/bar.conf.d/a.conf:b = b /CONFD/foo/bar.conf.d/a.conf
// foo/bar.conf.d/a.conf:c = c /CONFD/foo/bar.conf.d/a.conf
//
// foo/bar.conf.d/b.conf:b = b /CONFD/foo/bar.conf.d/b.conf
// foo/bar.conf.d/b.conf:c = c /CONFD/foo/bar.conf.d/b.conf
//
// foo/bar.conf.d/c.conf:c = c /CONFD/foo/bar.conf.d/c.conf
//
// foo/bar.conf.d/noext:noext = OOps

BOOST_AUTO_TEST_CASE(full)
{
  // Full range of config files on all layers, so /etc wins:
  // full/etc/foo/bar.conf                * 1 abcd
  // full/etc/foo/bar.conf.d/a.conf       * 2 abc
  // full/etc/foo/bar.conf.d/b.conf       * 3  bc
  // full/etc/foo/bar.conf.d/c.conf       * 4   c
  // full/etc/foo/bar.conf.d/noext
  EconfDict a( "foo/bar.conf", DATADIR / "full" );
  std::string got = str::sprint( dump(a) );
  std::string want = str::sprintf( str::FormatList, "[]"
  , "a = a /etc/foo/bar.conf.d/a.conf"
  , "b = b /etc/foo/bar.conf.d/b.conf"
  , "c = c /etc/foo/bar.conf.d/c.conf"
  , "d = d /etc/foo/bar.conf"
  );
  BOOST_CHECK_EQUAL( got, want );
}

BOOST_AUTO_TEST_CASE(half)
{
  // Full tree, but a few files missing on each layer:
  // half/etc/foo/bar.conf.d/a.conf       * 2 abc
  // half/etc/foo/bar.conf.d/c.conf       * 4   c
  // half/etc/foo/bar.conf.d/noext
  // half/run/foo/bar.conf.d/a.conf
  // half/run/foo/bar.conf.d/b.conf       * 3  bc
  // half/run/foo/bar.conf.d/c.conf
  // half/run/foo/bar.conf.d/noext
  // half/usr/etc/foo/bar.conf            * 1 abcd
  // half/usr/etc/foo/bar.conf.d/a.conf
  // half/usr/etc/foo/bar.conf.d/b.conf
  // half/usr/etc/foo/bar.conf.d/c.conf
  // half/usr/etc/foo/bar.conf.d/noext
  EconfDict a( "foo/bar.conf", DATADIR / "half" );
  std::string got = str::sprint( dump(a) );
  std::string want = str::sprintf( str::FormatList, "[]"
  , "a = a /etc/foo/bar.conf.d/a.conf"
  , "b = b /run/foo/bar.conf.d/b.conf"
  , "c = c /etc/foo/bar.conf.d/c.conf"
  , "d = d /usr/etc/foo/bar.conf"
  );
  BOOST_CHECK_EQUAL( got, want );
}

BOOST_AUTO_TEST_CASE(masked)
{
  // Like "half" but /etc masks it's missing files:
  // masked/etc/foo/bar.conf                * 1 ---- (empty file)
  // masked/etc/foo/bar.conf.d/a.conf       * 2 abc
  // masked/etc/foo/bar.conf.d/b.conf       * 3 ---- (-> /dev/null)
  // masked/etc/foo/bar.conf.d/c.conf       * 4   c
  EconfDict a( "foo/bar.conf", DATADIR / "masked" );
  std::string got = str::sprint( dump(a) );
  std::string want = str::sprintf( str::FormatList, "[]"
  , "a = a /etc/foo/bar.conf.d/a.conf"
  , "b = b /etc/foo/bar.conf.d/a.conf"
  , "c = c /etc/foo/bar.conf.d/c.conf"
  );
  BOOST_CHECK_EQUAL( got, want );
}
