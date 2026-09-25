/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/**
 * contextconfig.cc — module implementation unit for zyppng:contextconfig.
 *
 * Provides the canonical ContextConfig factory functions:
 *
 *   makeSystemConfig()       — for the running system ("/")
 *   makeRootConfig(path)     — for an alternate root (e.g. "/mnt")
 *   makeDefaultConfig()      — defaults only, no file I/O (tests)
 *
 * Each factory follows the explicit sequence:
 *   1. Construct Config(root)
 *   2. registerZyppKeys(cfg)   — populate the registry
 *   3. cfg.parse()             — read zypp.conf, returns ParseErrors
 *
 * No static-init tricks, no global callbacks.  Ordering is enforced
 * structurally by the factory body.
 */
module;
#include <zypp/ng/config/zyppconfig.h>

module zyppng;

import :contextconfig;

namespace zyppng {

// ---------------------------------------------------------------------------
// ContextConfig factories
// ---------------------------------------------------------------------------

std::pair<ContextConfig, std::vector<zyppng::config::ParseError>>
makeSystemConfig()
{
    ContextConfig cfg( std::filesystem::path("/") );
    ::zyppng::config::registerZyppKeys( cfg );
    auto errors = cfg.parse();
    return { std::move(cfg), std::move(errors) };
}

std::pair<ContextConfig, std::vector<zyppng::config::ParseError>>
makeRootConfig( std::filesystem::path root )
{
    ContextConfig cfg( std::move(root) );
    ::zyppng::config::registerZyppKeys( cfg );
    auto errors = cfg.parse();
    return { std::move(cfg), std::move(errors) };
}

ContextConfig makeDefaultConfig()
{
    // No parse — returns a Config that yields compiled-in defaults for
    // all registered keys.  Useful for tests and synthetic contexts.
    ContextConfig cfg( std::filesystem::path("/") );
    ::zyppng::config::registerZyppKeys( cfg );
    return cfg;
}

} // namespace zyppng
