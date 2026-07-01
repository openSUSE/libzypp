/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
module;
#include <filesystem>
#include <mutex>
#include <zypp-core/fs/TmpPath.h>
#include <zypp-core/ng/base/private/base_p.h>
#include <zypp-core/ng/ui/private/userinterface_p.h>
#include <zypp-core/ng/base/zypp-core-ng-fwd.h>
#include <zypp-media/ng/providefwd.h>

export module zyppng:context_private;  // internal — not re-exported from zyppng.cppm

import :context;
import :contextconfig;

export namespace zyppng
{

  class ContextPrivate : public UserInterfacePrivate
  {
  public:
    ContextPrivate ( Context &b, std::filesystem::path root )
      : UserInterfacePrivate(b)
      , _config( std::move(root) )
    {}
    ~ContextPrivate();
    EventDispatcherRef _eventDispatcher;
    zypp::filesystem::TmpDir _providerDir;
    ProvideRef _provider;
    ContextConfig _config;
  };
}

