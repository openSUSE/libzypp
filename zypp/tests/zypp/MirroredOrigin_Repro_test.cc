#define BOOST_TEST_MODULE MirroredOrigin Repro Suite
#include <boost/test/unit_test.hpp>
#include <zypp-core/Url.h>
#include <zypp-core/MirroredOrigin.h>

using namespace zypp;

BOOST_AUTO_TEST_CASE(repro_set_authority_incompatible_mirrors)
{
    OriginEndpoint http_auth{Url("http://auth.com")};
    OriginEndpoint http_mirror{Url("http://mirror.com")};
    OriginEndpoint file_auth{Url("file:/local/path")};

    MirroredOrigin origin(http_auth, {http_mirror});
    BOOST_REQUIRE_EQUAL(origin.mirrors().size(), 1);

    // BUG: Changing to file:/ authority should drop http:// mirrors.
    // Currently it doesn't because (newAuthIsDl && !i->schemeIsDownloading()) is false.
    origin.setAuthority(file_auth);
    
    BOOST_CHECK_EQUAL(origin.authority(), file_auth);
    // This is expected to FAIL: mirrors will NOT be empty
    BOOST_CHECK_EQUAL(origin.mirrors().size(), 0);
}

BOOST_AUTO_TEST_CASE(repro_add_incompatible_mirror_to_file_authority)
{
    OriginEndpoint file_auth{Url("file:/local/path")};
    OriginEndpoint http_mirror{Url("http://mirror.com")};

    MirroredOrigin origin(file_auth);
    
    // BUG: Adding http:// mirror to file:/ authority should fail.
    // Currently it succeeds because (authIsDl && !newMirror.schemeIsDownloading()) is false.
    bool added = origin.addMirror(http_mirror);
    
    // This is expected to FAIL: added will be true
    BOOST_CHECK_EQUAL(added, false);
    BOOST_CHECK_EQUAL(origin.mirrors().size(), 0);
}

BOOST_AUTO_TEST_CASE(repro_add_incompatible_authority_to_file_authority)
{
    OriginEndpoint file_auth{Url("file:/local/path")};
    OriginEndpoint http_auth{Url("http://auth.com")};

    MirroredOrigin origin(file_auth);
    
    // BUG: Adding http:// authority to file:/ authority should fail.
    // Currently it succeeds because (authIsDl && !newAuthority.schemeIsDownloading()) is false.
    bool added = origin.addAuthority(http_auth);
    
    // This is expected to FAIL: added will be true
    BOOST_CHECK_EQUAL(added, false);
    BOOST_CHECK_EQUAL(origin.authorities().size(), 1);
}
