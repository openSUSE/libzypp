
#include <iostream>
#include <fstream>
#include <list>
#include <string>

#include <zypp-core/base/Logger.h>
#include <zypp-core/base/Exception.h>
#include <zypp-common/PublicKey.h>
#include <zypp-common/KeyManager.h>
#include <zypp/TmpPath.h>
#include <zypp-core/Date.h>

#include <boost/test/unit_test.hpp>

using boost::unit_test::test_suite;
using boost::unit_test::test_case;

using namespace zypp;

#define DATADIR (Pathname(TESTS_SRC_DIR) +  "/zypp/data/PublicKey")

BOOST_AUTO_TEST_CASE(publickey_test)
{
  // test for a empty key
  zypp::PublicKey empty_key;
  BOOST_REQUIRE( ! empty_key.isValid() );

  BOOST_CHECK_THROW( zypp::PublicKey("nonexistent"), Exception );

  zypp::PublicKey k2(DATADIR/"susekey.asc");
  BOOST_CHECK_EQUAL( k2.id(), "A84EDAE89C800ACA" );
  BOOST_CHECK_EQUAL( k2.name(), "SuSE Package Signing Key <build@suse.de>" );
  BOOST_CHECK_EQUAL( k2.fingerprint(), "79C179B2E1C820C1890F9994A84EDAE89C800ACA" );
  BOOST_CHECK_EQUAL( k2.gpgPubkeyVersion(), "9c800aca" );
  BOOST_CHECK_EQUAL( k2.gpgPubkeyRelease(), "40d8063e" );
  BOOST_CHECK_EQUAL( k2.created(), zypp::Date(1087899198) );
  BOOST_CHECK_EQUAL( k2.expires(), zypp::Date(1214043198) );
//BOOST_CHECK_EQUAL( k2.daysToLive(), "" );
  BOOST_REQUIRE( k2.path() != Pathname() );
  BOOST_REQUIRE( k2 == k2 );

  k2 = zypp::PublicKey(DATADIR/"multikey.asc");
  BOOST_CHECK_EQUAL( k2.id(), "27FA41BD8A7C64F9" );
  BOOST_CHECK_EQUAL( k2.name(), "Unsupported <unsupported@suse.de>" );
  BOOST_CHECK_EQUAL( k2.fingerprint(), "D88811AF6B51852351DF538527FA41BD8A7C64F9" );
  BOOST_CHECK_EQUAL( k2.gpgPubkeyVersion(), "8a7c64f9" );
  BOOST_CHECK_EQUAL( k2.gpgPubkeyRelease(), "4be01af3" );
  BOOST_CHECK_EQUAL( k2.created(), zypp::Date(1272978163) );
  BOOST_CHECK_EQUAL( k2.expires(), zypp::Date(1399122163) );

  k2 = zypp::PublicKey(DATADIR/"multikey2.asc");
  BOOST_CHECK_EQUAL( k2.hiddenKeys().size(), 8 );
}

BOOST_AUTO_TEST_CASE(keymanager_volatile_context_test)
{
  auto ctx1 = KeyManagerCtx::createForOpenPGP();
  auto ctx2 = KeyManagerCtx::createForOpenPGP();

  BOOST_REQUIRE( ctx1.homedir() != Pathname() );
  BOOST_REQUIRE( ctx2.homedir() != Pathname() );
  BOOST_CHECK( ctx1.homedir() != ctx2.homedir() );

  BOOST_CHECK_EQUAL( ctx1.listKeys().size(), 0U );
  BOOST_CHECK_EQUAL( ctx2.listKeys().size(), 0U );

  auto keys1 = ctx1.readKeyFromFile( DATADIR/"susekey.asc" );
  BOOST_REQUIRE_EQUAL( keys1.size(), 1U );
  BOOST_CHECK_EQUAL( ctx1.listKeys().size(), 1U );
  BOOST_CHECK_EQUAL( ctx2.listKeys().size(), 0U );

  auto keys2 = ctx1.readKeyFromFile( DATADIR/"multikey2.asc" );
  BOOST_REQUIRE_EQUAL( keys2.size(), 9U );
  BOOST_CHECK_EQUAL( ctx1.listKeys().size(), 9U );
  BOOST_CHECK_EQUAL( ctx2.listKeys().size(), 0U );
}
