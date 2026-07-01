/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
module;
#include <zypp-core/ng/base/private/threaddata_p.h>
#include <zypp-core/ng/base/EventLoop>
#include <zypp-core/base/LogTools.h>
#include <zypp-media/ng/Provide>
#include <zypp/ng/config/zyppconfig.h>

module zyppng;

import :context;
import :context_private;
import :contextconfig;

namespace zyppng {

  namespace {
    void logConfigParseErrors( const std::vector<zyppng::config::ParseError> & errors )
    {
      for ( const auto & e : errors ) {
        try { std::rethrow_exception( e.error ); }
        catch ( const std::exception & ex ) {
          WAR << "zypp.conf: key '" << e.key << "' value '" << e.rawValue
              << "': " << ex.what() << " (using compiled-in default)" << std::endl;
        }
      }
    }
  }

  ContextPrivate::~ContextPrivate()
  {}

  ZYPP_IMPL_PRIVATE( Context )

  ZYPP_IMPL_PRIVATE_CONSTR_ARGS( Context, std::filesystem::path root )
    : UserInterface( *new ContextPrivate( *this, root ) )
  {
    Z_D();
    d->_eventDispatcher = ThreadData::current().ensureDispatcher();

    d->_provider = Provide::create( d->_providerDir );
    d->_provider->start();

    // Register all domain keys then parse zypp.conf for this root.
    // Parse errors are logged as warnings; the config remains fully usable
    // (keys that fail fall through to their compiled-in defaults).
    ::zyppng::config::registerZyppKeys( d->_config );
    logConfigParseErrors( d->_config.parse() );
  }

  ProvideRef Context::provider() const
  {
    Z_D();
    return d->_provider;
  }

  ContextConfig & Context::config()
  {
    Z_D();
    return d->_config;
  }

  const ContextConfig & Context::config() const
  {
    Z_D();
    return d->_config;
  }

}
