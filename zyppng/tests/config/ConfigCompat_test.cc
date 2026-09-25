#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include <zypp/ng/config/zyppconfig.h>
#include <zypp-core/fs/TmpPath.h>
#include <zypp-core/fs/PathInfo.h>

import zyppng;

using namespace zyppng;
using namespace zyppng::config;

// ---------------------------------------------------------------------------
// Compatibility smoke test
//
// Verifies that every key in zypp.conf.LEGACY is recognised by the registry
// (i.e. no key falls through to raw string storage).  A failure here means
// a key was added to zypp.conf but not registered in registerZyppKeys(), or
// an alias mapping is missing.
// ---------------------------------------------------------------------------

BOOST_AUTO_TEST_SUITE(ConfigCompatTest)

BOOST_AUTO_TEST_CASE(legacy_conf_all_keys_recognised)
{
    // Install zypp.conf.LEGACY as etc/zypp/zypp.conf under a temp root.
    zypp::filesystem::TmpDir root;
    zypp::Pathname confDir = root.path() / "etc/zypp";
    ::zypp::filesystem::assert_dir( confDir );

    zypp::Pathname src( TESTS_SRC_DIR "/data/zypp.conf.ALL_KEYS" );
    zypp::Pathname dst = confDir / "zypp.conf";
    BOOST_REQUIRE_MESSAGE( zypp::filesystem::copy( src, dst ) == 0,
        "Failed to copy zypp.conf.LEGACY to " << dst );

    // Construct, register, parse.
    ContextConfig cfg( root.path().asString() );
    registerZyppKeys( cfg );
    auto errors = cfg.parse();

    // ── No parse errors ────────────────────────────────────────────────────
    // Every active (uncommented) value in zypp.conf.LEGACY must parse cleanly.
    for ( const auto & e : errors ) {
        try { std::rethrow_exception( e.error ); }
        catch ( const std::exception & ex ) {
            BOOST_ERROR( "Parse error for key '" << e.key
                << "' value '" << e.rawValue << "': " << ex.what() );
        }
    }
    BOOST_CHECK_EQUAL( errors.size(), 0u );

    // ── No raw keys ────────────────────────────────────────────────────────
    // Every key in the file must have been recognised by the registry.
    // A raw/unknown key is stored as std::string AND has no registered parseFn.
    // Registered Key<std::string> keys (e.g. UpdateMessagesNotify) also store
    // std::string but ARE in the registry — they must not be flagged.
    std::vector<std::string> rawKeys;
    cfg.forEach([&]( std::string_view key, const Entry & entry ) {
        if ( entry.fileValue.has_value()
             && entry.fileValue.type() == typeid(std::string)
             && key.find("preview/") == std::string_view::npos
             && !cfg.hasRegisteredKey(key) ) {
            rawKeys.push_back( std::string(key) );
        }
    });

    for ( const auto & k : rawKeys )
        BOOST_ERROR( "Key '" << k << "' was stored as raw string — "
            "missing from registerZyppKeys() or alias map" );

    BOOST_CHECK_EQUAL( rawKeys.size(), 0u );
}

BOOST_AUTO_TEST_SUITE_END()
