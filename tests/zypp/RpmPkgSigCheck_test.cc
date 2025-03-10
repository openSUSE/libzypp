#include "TestSetup.h"

#include <zypp/target/rpm/RpmDb.h>
using target::rpm::RpmDb;

#define DATADIR (Pathname(TESTS_SRC_DIR) / "/zypp/data/RpmPkgSigCheck")

static TestSetup test( TestSetup::initLater );
struct TestInit {
  TestInit() {
    test = TestSetup( );
  }
  ~TestInit() { test.reset(); }
};
BOOST_GLOBAL_FIXTURE( TestInit );

///////////////////////////////////////////////////////////////////
//
// - RpmDb::checkPackage (legacy) and RpmDb::checkPackageSignature are
// expected to produce the same result, except for ...
//
// Result comparison is not very sophisticated. As the detail strings are
// user visible (at least in zypper) we want a notification (breaking testcase)
// if something in the rpm format changes.
//
///////////////////////////////////////////////////////////////////
namespace
{
  struct CheckResult
  {
    CheckResult()
    {}

    CheckResult( RpmDb::CheckPackageResult && result_r )
    : result { std::move(result_r) }
    {}

    CheckResult( RpmDb::CheckPackageResult && result_r,
                 std::vector<std::pair<RpmDb::CheckPackageResult,std::string>> && detail_r )
    : result { std::move(result_r) }
    { static_cast<std::vector<std::pair<RpmDb::CheckPackageResult,std::string>>&>(detail) = std::move(detail_r); }

    RpmDb::CheckPackageResult result;
    RpmDb::CheckPackageDetail detail;
  };

  bool operator==( const CheckResult & lhs, const CheckResult & rhs )
  {
    if ( lhs.result != rhs.result )
      return false;
    // protect against reordered details:

    // there seems to be a backporting of how rpm prints the signature check result
    // breaking our tests here, instead of checking for exact equality we just require
    // that all elements in the lhs exist in the rhs instance.
    //if ( lhs.detail.size() != rhs.detail.size() )
    //  return false;

    for ( const auto & l : lhs.detail )
    {
      if ( std::find( rhs.detail.begin(), rhs.detail.end(), l ) == rhs.detail.end() )
        return false;
    }
    return true;
  }

  std::ostream & operator<<( std::ostream & str, const CheckResult & obj )
  {
    str << "R: " << obj.result;
    for ( const auto & el : obj.detail )
      str << endl << "   "  << el.first << " | " << el.second;
    return str;
  }

  CheckResult gcheckPackage( const Pathname & path_r )
  {
    CheckResult res;
    res.result = test.target().rpmDb().checkPackage( path_r, res.detail );
//     cout << "==-" << path_r << endl;
//     cout << res << endl;
    return res;
  }

  CheckResult gcheckPackageSignature( const Pathname & path_r )
  {
    CheckResult res;
    res.result = test.target().rpmDb().checkPackageSignature( path_r, res.detail );
//     cout << "==!" << path_r << endl;
//     cout << res << endl;
    return res;
  }
} // namespace


BOOST_AUTO_TEST_CASE(no_pkg)
{
  Pathname rpm { DATADIR/"no.rpm" };
  CheckResult cp { gcheckPackage( rpm ) };
  CheckResult cs { gcheckPackageSignature( rpm ) };
  BOOST_CHECK_EQUAL( cp, cs );

  CheckResult xpct { RpmDb::CHK_ERROR, {/*empty details*/} };
  BOOST_CHECK_EQUAL( xpct, cs );
}

BOOST_AUTO_TEST_CASE(unsigned_pkg)
{
  Pathname rpm { DATADIR/"unsigned.rpm" };
  CheckResult cp { gcheckPackage( rpm ) };
  CheckResult cs { gcheckPackageSignature( rpm ) };
  // For unsigned packages the final result differs!
  // (but only if the digests are OK)
  BOOST_CHECK_EQUAL( cp.result, RpmDb::CHK_OK );
  BOOST_CHECK_EQUAL( cs.result, RpmDb::CHK_NOSIG );
  BOOST_CHECK_EQUAL( cp.detail, cs.detail );

  CheckResult xpct { RpmDb::CHK_NOSIG, {
    { RpmDb::CHK_OK,	"    Header SHA1 digest: OK" },
    { RpmDb::CHK_OK,	"    Header SHA256 digest: OK" },
    { RpmDb::CHK_OK,	"    Payload SHA256 digest: OK" },
    { RpmDb::CHK_OK,	"    MD5 digest: OK" },
    { RpmDb::CHK_NOSIG,	"    Package header is not signed!" },
  } };
  BOOST_CHECK_EQUAL( xpct, cs );
}

BOOST_AUTO_TEST_CASE(unsigned_broken_pkg)
{
  Pathname rpm { DATADIR/"unsigned_broken.rpm" };
  CheckResult cp { gcheckPackage( rpm ) };
  CheckResult cs { gcheckPackageSignature( rpm ) };
  // Unsigned, but a broken digest 'superseeds' CHK_NOSIG
  BOOST_CHECK_EQUAL( cp, cs );

  CheckResult xpct { RpmDb::CHK_FAIL, {
    { RpmDb::CHK_OK,	"    Header SHA1 digest: OK" },
    { RpmDb::CHK_OK,	"    Header SHA256 digest: OK" },
    { RpmDb::CHK_FAIL,	"    Payload SHA256 digest: BAD (Expected 6632dfb6e78fd3346baa860da339acdedf6f019fb1b5448ba1baa6cef67de795 != 85156c232f4c76273bbbb134d8d869e93bbfc845dd0d79016856e5356dd33727)" },
    { RpmDb::CHK_FAIL,	"    MD5 digest: BAD (Expected e3f474f75d2d2b267da4ff80fc071dd7 != cebe1e7d39b4356639a0779aa23f6e27)" },
  } };
  BOOST_CHECK_EQUAL( xpct, cs );
}

BOOST_AUTO_TEST_CASE(unsigned_broken_header_pkg)
{
  Pathname rpm { DATADIR/"unsigned_broken_header.rpm" };
  CheckResult cp { gcheckPackage( rpm ) };
  CheckResult cs { gcheckPackageSignature( rpm ) };
  // Unsigned, but a broken digest 'superseeds' CHK_NOSIG
  BOOST_CHECK_EQUAL( cp, cs );

  CheckResult xpct { RpmDb::CHK_FAIL, {
    { RpmDb::CHK_FAIL,	"    Header SHA1 digest: BAD (Expected d6768447e13388b0c35fb151ebfa8f6646a115e9 != dd761ace671a5eb2669b269faf22a3cd72792138)" },
    { RpmDb::CHK_FAIL,	"    Header SHA256 digest: BAD (Expected 2ce9f41bc0de68b4cb1aa1e18c1bea43dfaa01299ae61ef3e4466df332c792e5 != 4a9410db7131cead773afe1876f2490023ccc7dc47cbba47807430c53ea9649d)" },
    { RpmDb::CHK_FAIL,	"    Payload SHA256 digest: BAD (Expected 6632dfb6e78fd3346baa860da339acdedf6f019fb1b5448ba1baa6cef67de795 != 85156c232f4c76273bbbb134d8d869e93bbfc845dd0d79016856e5356dd33727)" },
    { RpmDb::CHK_FAIL,	"    MD5 digest: BAD (Expected e3f474f75d2d2b267da4ff80fc071dd7 != 9afd6b52896d23910280ddded1921071)" },
  } };
  BOOST_CHECK_EQUAL( xpct, cs );
}

BOOST_AUTO_TEST_CASE(signed_pkg_nokey)
{
  Pathname rpm { DATADIR/"signed.rpm" };
  CheckResult cp { gcheckPackage( rpm ) };
  CheckResult cs { gcheckPackageSignature( rpm ) };
  BOOST_CHECK_EQUAL( cp, cs );

  CheckResult xpct { RpmDb::CHK_NOKEY, {
    { RpmDb::CHK_NOKEY,	"    Header V4 RSA/SHA256 Signature, key ID e279209bbb65a216: NOKEY" },
    { RpmDb::CHK_OK,	"    Header SHA1 digest: OK" },
    { RpmDb::CHK_OK,	"    Header SHA256 digest: OK" },
    { RpmDb::CHK_OK,	"    Payload SHA256 digest: OK" },
    { RpmDb::CHK_OK,	"    MD5 digest: OK" },
    { RpmDb::CHK_NOKEY,	"    V4 RSA/SHA256 Signature, key ID e279209bbb65a216: NOKEY" },
  } };
  BOOST_CHECK_EQUAL( xpct, cs );
}

BOOST_AUTO_TEST_CASE(signed_broken_pkg_nokey)
{
  Pathname rpm { DATADIR/"signed_broken.rpm" };
  CheckResult cp { gcheckPackage( rpm ) };
  CheckResult cs { gcheckPackageSignature( rpm ) };
  BOOST_CHECK_EQUAL( cp, cs );

  CheckResult xpct { RpmDb::CHK_FAIL, {
    { RpmDb::CHK_NOKEY,	"    Header V4 RSA/SHA256 Signature, key ID e279209bbb65a216: NOKEY" },
    { RpmDb::CHK_OK,	"    Header SHA1 digest: OK" },
    { RpmDb::CHK_OK,	"    Header SHA256 digest: OK" },
    { RpmDb::CHK_FAIL,	"    Payload SHA256 digest: BAD (Expected 6632dfb6e78fd3346baa860da339acdedf6f019fb1b5448ba1baa6cef67de795 != 85156c232f4c76273bbbb134d8d869e93bbfc845dd0d79016856e5356dd33727)" },
    { RpmDb::CHK_FAIL,	"    MD5 digest: BAD (Expected 8e64684e4d5bd90c3c13f76ecbda9ee2 != 442a473472708c39f3ac2b5eb38b476f)" },
    { RpmDb::CHK_FAIL,	"    V4 RSA/SHA256 Signature, key ID e279209bbb65a216: BAD" },
  } };
  BOOST_CHECK_EQUAL( xpct, cs );
}

BOOST_AUTO_TEST_CASE(signed_broken_header_pkg_nokey)
{
  Pathname rpm { DATADIR/"signed_broken_header.rpm" };
  CheckResult cp { gcheckPackage( rpm ) };
  CheckResult cs { gcheckPackageSignature( rpm ) };
  BOOST_CHECK_EQUAL( cp, cs );

  CheckResult xpct { RpmDb::CHK_FAIL, {
    { RpmDb::CHK_FAIL,	"    Header V4 RSA/SHA256 Signature, key ID e279209bbb65a216: BAD" },
    { RpmDb::CHK_FAIL,	"    Header SHA1 digest: BAD (Expected 9ca2e0aec038e562d33442271ee52c08ded0d637 != 9ca2e3aec038e562d33442271ee52c08ded0d637)" },
    { RpmDb::CHK_FAIL,	"    Header SHA256 digest: BAD (Expected e8810065608e06b6e4bb9155f0dd111ef8042866941f02b623cb46e12a82f732 != e88100656c8e06b6e4bb9155f0dd111ef8042866941f02b623cb46e12a82f732)" },
    { RpmDb::CHK_FAIL,	"    Payload SHA256 digest: BAD (Expected 6632dfb6e78fd3346baa860da339acdedf6f019fb1b5448ba1baa6cef67de795 != 85156c232f4c76273bbbb134d8d869e93bbfc845dd0d79016856e5356dd33727)" },
    { RpmDb::CHK_FAIL,	"    MD5 digest: BAD (Expected 8e64684e4d5bd90c3c13f76ecbda9ee2 != 442a473472708c39f3ac2b5eb38b476f)" },
    { RpmDb::CHK_FAIL,	"    V4 RSA/SHA256 Signature, key ID e279209bbb65a216: BAD" },
  } };
  BOOST_CHECK_EQUAL( xpct, cs );
}

///////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(add_key)
{
  PublicKey key { Pathname(DATADIR)/"signed.key" };
  //cout << key << endl;
  test.target().rpmDb().importPubkey( key );
}
///////////////////////////////////////////////////////////////////


BOOST_AUTO_TEST_CASE(signed_pkg_withkey)
{
  Pathname rpm { DATADIR/"signed.rpm" };
  CheckResult cp { gcheckPackage( rpm ) };
  CheckResult cs { gcheckPackageSignature( rpm ) };
  BOOST_CHECK_EQUAL( cp, cs );

  CheckResult xpct { RpmDb::CHK_OK, {
    { RpmDb::CHK_OK,	"    Header V4 RSA/SHA256 Signature, key ID bb65a216: OK" },
    { RpmDb::CHK_OK,	"    Header SHA1 digest: OK" },
    { RpmDb::CHK_OK,	"    Header SHA256 digest: OK" },
    { RpmDb::CHK_OK,	"    Payload SHA256 digest: OK" },
    { RpmDb::CHK_OK,	"    MD5 digest: OK" },
    { RpmDb::CHK_OK,	"    V4 RSA/SHA256 Signature, key ID bb65a216: OK" },
  } };
  BOOST_CHECK_EQUAL( xpct, cs );
}

BOOST_AUTO_TEST_CASE(signed_broken_pkg_withkey)
{
  Pathname rpm { DATADIR/"signed_broken.rpm" };
  CheckResult cp { gcheckPackage( rpm ) };
  CheckResult cs { gcheckPackageSignature( rpm ) };
  BOOST_CHECK_EQUAL( cp, cs );

  CheckResult xpct { RpmDb::CHK_FAIL, {
    { RpmDb::CHK_OK,	"    Header V4 RSA/SHA256 Signature, key ID e279209bbb65a216: OK" },
    { RpmDb::CHK_OK,	"    Header SHA1 digest: OK" },
    { RpmDb::CHK_OK,	"    Header SHA256 digest: OK" },
    { RpmDb::CHK_FAIL,	"    Payload SHA256 digest: BAD (Expected 6632dfb6e78fd3346baa860da339acdedf6f019fb1b5448ba1baa6cef67de795 != 85156c232f4c76273bbbb134d8d869e93bbfc845dd0d79016856e5356dd33727)" },
    { RpmDb::CHK_FAIL,	"    MD5 digest: BAD (Expected 8e64684e4d5bd90c3c13f76ecbda9ee2 != 442a473472708c39f3ac2b5eb38b476f)" },
    { RpmDb::CHK_FAIL,	"    V4 RSA/SHA256 Signature, key ID e279209bbb65a216: BAD" },
  } };
  BOOST_CHECK_EQUAL( xpct, cs );
}

BOOST_AUTO_TEST_CASE(signed_broken_header_pkg_withkey)
{
  Pathname rpm { DATADIR/"signed_broken_header.rpm" };
  CheckResult cp { gcheckPackage( rpm ) };
  CheckResult cs { gcheckPackageSignature( rpm ) };
  BOOST_CHECK_EQUAL( cp, cs );

  CheckResult xpct { RpmDb::CHK_FAIL, {
    { RpmDb::CHK_FAIL,	"    Header V4 RSA/SHA256 Signature, key ID e279209bbb65a216: BAD" },
    { RpmDb::CHK_FAIL,	"    Header SHA1 digest: BAD (Expected 9ca2e0aec038e562d33442271ee52c08ded0d637 != 9ca2e3aec038e562d33442271ee52c08ded0d637)" },
    { RpmDb::CHK_FAIL,	"    Header SHA256 digest: BAD (Expected e8810065608e06b6e4bb9155f0dd111ef8042866941f02b623cb46e12a82f732 != e88100656c8e06b6e4bb9155f0dd111ef8042866941f02b623cb46e12a82f732)" },
    { RpmDb::CHK_FAIL,	"    Payload SHA256 digest: BAD (Expected 6632dfb6e78fd3346baa860da339acdedf6f019fb1b5448ba1baa6cef67de795 != 85156c232f4c76273bbbb134d8d869e93bbfc845dd0d79016856e5356dd33727)" },
    { RpmDb::CHK_FAIL,	"    MD5 digest: BAD (Expected 8e64684e4d5bd90c3c13f76ecbda9ee2 != 442a473472708c39f3ac2b5eb38b476f)" },
    { RpmDb::CHK_FAIL,	"    V4 RSA/SHA256 Signature, key ID e279209bbb65a216: BAD" },
  } };
  BOOST_CHECK_EQUAL( xpct, cs );
}
