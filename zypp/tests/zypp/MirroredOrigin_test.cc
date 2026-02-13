// This define is necessary to automatically generate a main() function for the test executable
#define BOOST_TEST_MODULE MirroredOrigin Suite

#include <boost/test/unit_test.hpp>
#include <iostream>
#include <list>
#include <stdexcept>


#include <zypp-core/Url.h>
#include <zypp-core/MirroredOrigin.h>

using namespace zypp;

// Test suite for the entire MirroredOrigin feature
BOOST_AUTO_TEST_SUITE(mirrored_origin_feature_suite)

// Common test data
struct TestData {
    OriginEndpoint http_auth{Url("http://primary.com/repo")};
    OriginEndpoint http_mirror1{Url("http://mirror1.com/repo")};
    OriginEndpoint https_auth{Url("https://secure.com/repo")};
    OriginEndpoint ftp_auth{Url("ftp://files.com/repo")};
    OriginEndpoint dvd_auth{Url("dvd:///mnt/dvd_repo")};       // Non-downloading
    OriginEndpoint file_auth{Url("file:/etc/zypp/repos.d/sles")}; // Non-downloading
    OriginEndpoint file_mirror{Url("file:/srv/dist/repo")};         // Non-downloading
};

//####################################################################
//## 1. Tests for OriginEndpoint
//####################################################################
BOOST_FIXTURE_TEST_SUITE(origin_endpoint_suite, TestData)

BOOST_AUTO_TEST_CASE(construction_and_accessors)
{
    BOOST_TEST_MESSAGE("Testing OriginEndpoint: Construction and Accessors...");
    BOOST_CHECK(http_auth.isValid());
    BOOST_CHECK_EQUAL(http_auth.url().asString(), "http://primary.com/repo");
    BOOST_CHECK_EQUAL(http_auth.scheme(), "http");
    BOOST_CHECK(http_auth.schemeIsDownloading());

    BOOST_CHECK(dvd_auth.isValid());
    BOOST_CHECK_EQUAL(dvd_auth.scheme(), "dvd");
    BOOST_CHECK(!dvd_auth.schemeIsDownloading());

    OriginEndpoint default_ep;
    BOOST_CHECK(!default_ep.isValid());
}


BOOST_AUTO_TEST_CASE(config_management)
{
    BOOST_TEST_MESSAGE("Testing OriginEndpoint: Config Management (std::any)...");
    OriginEndpoint ep{Url("http://test.com")};

    ep.setConfig<int>("priority", 10);
    ep.setConfig<std::string>("region", "EU");

    BOOST_CHECK(ep.hasConfig("priority"));
    BOOST_CHECK(ep.hasConfig("region"));
    BOOST_CHECK(!ep.hasConfig("nonexistent"));

    BOOST_CHECK_EQUAL(ep.getConfig<int>("priority"), 10);
    BOOST_CHECK_EQUAL(ep.getConfig<std::string>("region"), "EU");

    // Test bad cast
    BOOST_CHECK_THROW(ep.getConfig<double>("priority"), std::bad_any_cast);

    ep.eraseConfigValue("priority");
    BOOST_CHECK(!ep.hasConfig("priority"));
}

BOOST_AUTO_TEST_SUITE_END()


//####################################################################
//## 2. Tests for MirroredOrigin
//####################################################################
BOOST_FIXTURE_TEST_SUITE(mirrored_origin_suite, TestData)

BOOST_AUTO_TEST_CASE(construction_and_management)
{
    BOOST_TEST_MESSAGE("Testing MirroredOrigin: Construction and Management...");
    MirroredOrigin origin(http_auth, {http_mirror1});

    BOOST_CHECK(origin.isValid());
    BOOST_CHECK_EQUAL(origin.endpointCount(), 2);
    BOOST_CHECK_EQUAL(origin.authority(), http_auth);
    BOOST_CHECK_EQUAL(origin.mirrors().size(), 1);
    BOOST_CHECK_EQUAL(origin.mirrors()[0], http_mirror1);

    // Test adding another downloading mirror
    BOOST_CHECK_EQUAL( origin.addMirror(https_auth), true );
    BOOST_CHECK_EQUAL(origin.mirrors().size(), 2);

    // Adding a URL with non downloading scheme, should fail
    BOOST_CHECK_EQUAL( origin.addMirror(file_auth), false );

    // Test clearing mirrors
    origin.clearMirrors();
    BOOST_CHECK_EQUAL(origin.mirrors().size(), 0);
    BOOST_CHECK_EQUAL(origin.endpointCount(), 1);
}

BOOST_AUTO_TEST_CASE(allow_duplicate_url_as_mirror)
{
    BOOST_TEST_MESSAGE("Testing MirroredOrigin: Dropping duplicate URL as mirror...");
    MirroredOrigin origin(http_auth);

    // Add the same URL as mirror, it should be dropped
    BOOST_CHECK_EQUAL(origin.addMirror(http_auth), true);
    BOOST_CHECK_EQUAL(origin.endpointCount(), 1); 
    BOOST_CHECK_EQUAL(origin.mirrors().size(), 0);
}

BOOST_AUTO_TEST_CASE(clean_mirrors_on_auth_change)
{
    BOOST_TEST_MESSAGE("Testing MirroredOrigin: Mirror cleanup on authority change...");
    MirroredOrigin origin( OriginEndpoint(), {http_mirror1} );

    BOOST_CHECK(!origin.isValid());
    BOOST_CHECK_EQUAL(origin.endpointCount(), 2);
    BOOST_CHECK_EQUAL(origin.authority(), OriginEndpoint());

    BOOST_CHECK_EQUAL(origin.mirrors().size(), 1);
    BOOST_CHECK_EQUAL(origin.mirrors()[0], http_mirror1);

    // Adding a URL with non downloading scheme, should work because authority is not yet specified
    BOOST_CHECK_EQUAL( origin.addMirror(file_auth), true );
    BOOST_CHECK_EQUAL( origin.mirrors().size(), 2 );

    BOOST_CHECK_EQUAL( origin.addMirror(dvd_auth), true );
    BOOST_CHECK_EQUAL( origin.mirrors().size(), 3 );

    // Test adding another downloading mirror
    BOOST_CHECK_EQUAL( origin.addMirror(https_auth), true );
    BOOST_CHECK_EQUAL( origin.mirrors().size(), 4 );

    // specifying the authority to be a downloading scheme should remove all non downloading ones
    origin.setAuthority( http_auth );
    BOOST_CHECK(origin.isValid());
    BOOST_CHECK(origin.schemeIsDownloading());
    BOOST_CHECK(origin.authority().schemeIsDownloading());
    BOOST_CHECK_EQUAL( origin.mirrors ().size(), 2 );

    // should retain original order
    BOOST_CHECK_EQUAL( origin[1], OriginEndpoint{http_mirror1} );
    BOOST_CHECK_EQUAL( origin[2], OriginEndpoint{https_auth} );
}


BOOST_AUTO_TEST_CASE(iteration_and_access)
{
    BOOST_TEST_MESSAGE("Testing MirroredOrigin: Iteration and Access...");
    MirroredOrigin origin(http_auth, {http_mirror1, ftp_auth});

    BOOST_CHECK_EQUAL(origin.endpointCount(), 3);

    // Check indexed access and iteration order (authority is always first)
    BOOST_CHECK_EQUAL(origin.at(0), http_auth);
    BOOST_CHECK_EQUAL(origin[1], http_mirror1);
    BOOST_CHECK_EQUAL(origin.at(2), ftp_auth);
    BOOST_CHECK_THROW(origin.at(3), std::out_of_range);

    // Check with range-based for loop
    int count = 0;
    for (const auto& ep : origin) {
        if (count == 0) BOOST_CHECK_EQUAL(ep, http_auth);
        else if (count == 1) BOOST_CHECK_EQUAL(ep, http_mirror1);
        count++;
    }
    BOOST_CHECK_EQUAL(count, 3);
}

BOOST_AUTO_TEST_SUITE_END()


//####################################################################
//## 3. Tests for MirroredOriginSet
//####################################################################
BOOST_FIXTURE_TEST_SUITE(mirrored_origin_set_suite, TestData)

BOOST_AUTO_TEST_CASE(grouping_logic)
{
    BOOST_TEST_MESSAGE("Testing MirroredOriginSet: Grouping Logic...");
    MirroredOriginSet sources;

    // 1. Add a downloading scheme
    sources.addEndpoint(http_auth);
    BOOST_CHECK_EQUAL(sources.size(), 1);
    BOOST_CHECK_EQUAL(sources.at(0).authority(), http_auth);

    // 2. Add a mirror for the same downloading scheme
    sources.addEndpoint(http_mirror1);
    BOOST_CHECK_EQUAL(sources.size(), 1); // Should not create a new MirroredOrigin
    BOOST_CHECK_EQUAL(sources.at(0).mirrors().size(), 1);

    // 3. Add a different downloading scheme ( http vs https )
    sources.addEndpoint(https_auth);
    BOOST_CHECK_EQUAL(sources.size(), 1); // Should not create a new MirroredOrigin
    BOOST_CHECK_EQUAL(sources.at(0).mirrors().size(), 2);

    // 4. Add a non-downloading scheme
    sources.addEndpoint(file_auth);
    BOOST_CHECK_EQUAL(sources.size(), 2); // Should create a new MirroredOrigin

    // 5. Add another non-downloading scheme of the same type
    // It should NOT be grouped, as per documentation.
    sources.addEndpoint(file_mirror);
    BOOST_CHECK_EQUAL(sources.size(), 3); // Should create another new MirroredOrigin
    BOOST_CHECK_EQUAL(sources.at(2).authority(), file_mirror);
}

BOOST_AUTO_TEST_CASE(order_stability)
{
    BOOST_TEST_MESSAGE("Testing MirroredOriginSet: Order Stability...");
    MirroredOriginSet sources;
    sources.addEndpoints({http_auth, dvd_auth, file_auth});

    BOOST_CHECK_EQUAL(sources.size(), 3);
    BOOST_CHECK_EQUAL(sources.at(0).scheme(), "http");
    BOOST_CHECK_EQUAL(sources.at(1).scheme(), "dvd");
    BOOST_CHECK_EQUAL(sources.at(2).scheme(), "file");
}

BOOST_AUTO_TEST_CASE(find_by_url)
{
    BOOST_TEST_MESSAGE("Testing MirroredOriginSet: findByUrl...");
    MirroredOriginSet sources;
    sources.addEndpoints({http_auth, http_mirror1, ftp_auth});

    // Find by authority URL
    auto it_auth = sources.findByUrl(http_auth.url());
    BOOST_CHECK(it_auth != sources.end());
    BOOST_CHECK_EQUAL(it_auth->authority(), http_auth);

    // Find by mirror URL
    auto it_mirror = sources.findByUrl(http_mirror1.url());
    BOOST_CHECK(it_mirror != sources.end());
    BOOST_CHECK_EQUAL(it_mirror->authority(), http_auth); // Should point to the same origin

    // Find non-existent URL
    auto it_none = sources.findByUrl(Url("http://nonexistent.com"));
    BOOST_CHECK(it_none == sources.end());
}

BOOST_AUTO_TEST_CASE(construction_from_range)
{
    BOOST_TEST_MESSAGE("Testing MirroredOriginSet: Construction from range...");
    std::vector<OriginEndpoint> endpoints = {http_auth, http_mirror1, dvd_auth};
    MirroredOriginSet sources(endpoints);

    BOOST_CHECK_EQUAL(sources.size(), 2);
    BOOST_CHECK_EQUAL(sources.at(0).scheme(), "http");
    BOOST_CHECK_EQUAL(sources.at(0).mirrors().size(), 1);
    BOOST_CHECK_EQUAL(sources.at(1).scheme(), "dvd");
}

BOOST_AUTO_TEST_CASE(iterator_functionality)
{
    BOOST_TEST_MESSAGE("Testing MirroredOriginSet: Iterator Functionality...");
    MirroredOriginSet sources;

    // 1. Test on empty set
    BOOST_CHECK(sources.begin() == sources.end());

    // 2. Populate the set
    sources.addEndpoints({http_auth, dvd_auth, file_auth});
    BOOST_CHECK_EQUAL(sources.size(), 3);

    // 3. Manual iteration and dereferencing
    auto it = sources.begin();
    BOOST_CHECK(it != sources.end());
    BOOST_CHECK_EQUAL(it->scheme(), "http");
    BOOST_CHECK_EQUAL((*it).authority(), http_auth);

    ++it;
    BOOST_CHECK(it != sources.end());
    BOOST_CHECK_EQUAL(it->scheme(), "dvd");

    ++it;
    BOOST_CHECK(it != sources.end());
    BOOST_CHECK_EQUAL(it->scheme(), "file");

    ++it;
    BOOST_CHECK(it == sources.end());

    // 4. Test with a range-based for loop
    std::vector<std::string> found_schemes;
    for (const MirroredOrigin& origin : sources) {
        found_schemes.push_back(origin.scheme());
    }
    std::vector<std::string> expected_schemes = {"http", "dvd", "file"};
    BOOST_CHECK_EQUAL_COLLECTIONS(found_schemes.begin(), found_schemes.end(),
                                  expected_schemes.begin(), expected_schemes.end());

    // 5. Test const iteration
    const MirroredOriginSet& const_sources = sources;
    std::vector<std::string> const_found_schemes;
    for (const auto& origin : const_sources) {
        const_found_schemes.push_back(origin.scheme());
    }
    BOOST_CHECK_EQUAL_COLLECTIONS(const_found_schemes.begin(), const_found_schemes.end(),
                                  expected_schemes.begin(), expected_schemes.end());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
