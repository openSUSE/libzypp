/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-curl/proxyinfo/proxyinfolibproxy.cc
 *
*/

#include <zypp-core/AutoDispose.h>
#include <iostream>
#include <optional>
#include <cstdlib>

#include <zypp-core/base/Logger.h>
#include <zypp-core/base/String.h>
#include <zypp-core/fs/WatchFile>
#include <zypp-core/Pathname.h>
#include <zypp/base/Env.h>

#include <zypp-curl/proxyinfo/ProxyInfoLibproxy>

#include <dlfcn.h> // for dlload, dlsym, dlclose
#include <glib.h>  // g_clear_pointer and g_strfreev

using std::endl;
using namespace zypp::base;

namespace zypp {
  namespace env {
    inline bool inYAST()
    {
      static const bool _inYAST { ::getenv("YAST_IS_RUNNING") };
      return _inYAST;
    }

    inline bool PX_DEBUG()
    {
      static const bool _pxdebug { env::getenvBool("PX_DEBUG") };
      return _pxdebug;
    }
  }

  namespace media {

    namespace {

      using CreateFactoryCb = CreateFactorySig<pxProxyFactoryType>;
      using DelFactoryCb    = DelFactorySig<pxProxyFactoryType>;
      using GetProxiesCb    = GetProxiesSig<pxProxyFactoryType>;

      struct LibProxyAPI
      {
        zypp::AutoDispose<void *> libProxyLibHandle;
        CreateFactoryCb createProxyFactory = nullptr;
        DelFactoryCb    deleteProxyFactory = nullptr;
        GetProxiesCb    getProxies         = nullptr;
        FreeProxiesCb   freeProxies        = nullptr;

        /*!
         * \internal
         * Older versions of libproxy do not have a callback to free the proxy list,
         * so we provide one.
         */
        static void fallbackFreeProxies( char **proxies ) {
          g_clear_pointer (&proxies, g_strfreev);
        }

        static std::unique_ptr<LibProxyAPI> create() {
          MIL << "Detecting libproxy availability" << std::endl;
          zypp::AutoDispose<void *> handle( dlopen("libproxy.so.1", RTLD_LAZY ), []( void *ptr ){ if ( ptr ) ::dlclose(ptr); });
          if ( !handle ) {
            MIL << "No libproxy support detected (could not load library): " << dlerror() << std::endl;
            return nullptr;
          }

          std::unique_ptr<LibProxyAPI> apiInstance = std::make_unique<LibProxyAPI>();
          apiInstance->libProxyLibHandle = std::move(handle);
          apiInstance->createProxyFactory = (CreateFactoryCb)::dlsym ( apiInstance->libProxyLibHandle, "px_proxy_factory_new" );
          if ( !apiInstance->createProxyFactory ){
            ERR << "Incompatible libproxy detected (could not resolve px_proxy_factory_new): " << dlerror() << std::endl;
            return nullptr;
          }
          apiInstance->deleteProxyFactory = (DelFactoryCb)::dlsym ( apiInstance->libProxyLibHandle, "px_proxy_factory_free" );
          if ( !apiInstance->deleteProxyFactory ){
            ERR << "Incompatible libproxy detected (could not resolve px_proxy_factory_free): " << dlerror() << std::endl;
            return nullptr;
          }
          apiInstance->getProxies = (GetProxiesCb)::dlsym ( apiInstance->libProxyLibHandle, "px_proxy_factory_get_proxies" );
          if ( !apiInstance->getProxies ){
            ERR << "Incompatible libproxy detected (could not resolve px_proxy_factory_get_proxies): " << dlerror() << std::endl;
            return nullptr;
          }
          apiInstance->freeProxies = (FreeProxiesCb)::dlsym ( apiInstance->libProxyLibHandle, "px_proxy_factory_free_proxies" );
          if ( !apiInstance->freeProxies ){
            MIL << "Older version of libproxy detected, using fallback function to free the proxy list (could not resolve px_proxy_factory_free_proxies): " << dlerror() << std::endl;
            apiInstance->freeProxies = &fallbackFreeProxies;
          }

          MIL << "Libproxy is available" << std::endl;
          return apiInstance;
        }
      };

      LibProxyAPI *proxyApi() {
        static std::unique_ptr<LibProxyAPI> api = LibProxyAPI::create();
        return api.get();
      }

      LibProxyAPI &assertProxyApi() {
        auto api = proxyApi();
        if ( !api )
          ZYPP_THROW( zypp::Exception("LibProxyAPI is not available.") );
        return *api;
      }
    }

    struct TmpUnsetEnv : private env::ScopedSet
    {
      TmpUnsetEnv( const char * var_r )
      : env::ScopedSet( var_r, nullptr )
      {}
    };

    static pxProxyFactoryType * getProxyFactory()
    {
      static pxProxyFactoryType * proxyFactory = 0;

      // Force libproxy into using "/etc/sysconfig/proxy"
      // if it exists.
      static WatchFile sysconfigProxy( "/etc/sysconfig/proxy", WatchFile::NO_INIT );
      if ( sysconfigProxy.hasChanged() )
      {
        MIL << "Build Libproxy Factory from /etc/sysconfig/proxy" << endl;
        if ( proxyFactory )
          assertProxyApi().deleteProxyFactory( proxyFactory );

        TmpUnsetEnv envguard[] __attribute__ ((__unused__)) = { "KDE_FULL_SESSION", "GNOME_DESKTOP_SESSION_ID", "DESKTOP_SESSION" };
        proxyFactory = assertProxyApi().createProxyFactory();
      }
      else if ( ! proxyFactory )
      {
        MIL << "Build Libproxy Factory" << endl;
        proxyFactory = assertProxyApi().createProxyFactory();
      }

      return proxyFactory;
    }

    ProxyInfoLibproxy::ProxyInfoLibproxy()
    : ProxyInfo::Impl()
    , _factory( getProxyFactory())
    {
      _enabled = !(_factory == NULL);
    }

    ProxyInfoLibproxy::~ProxyInfoLibproxy()
    {}

    bool ProxyInfoLibproxy::isAvailabe()
    {
      return ( proxyApi () != nullptr );
    }

    std::string ProxyInfoLibproxy::proxy(const Url & url_r) const
    {
      if (!_enabled)
        return "";

      const url::ViewOption vopt =
              url::ViewOption::WITH_SCHEME
              + url::ViewOption::WITH_HOST
              + url::ViewOption::WITH_PORT
              + url::ViewOption::WITH_PATH_NAME;

      auto &api = assertProxyApi ();

      zypp::AutoDispose<char **> proxies(
            api.getProxies(_factory, url_r.asString(vopt).c_str())
            , api.freeProxies
      );
      if ( !proxies.value() )
              return "";

      // bsc#1244710: libproxy appears to return /etc/sysconfig/proxy
      // values after $*_proxy environment variables.
      // In YAST, e.g after changing the proxy settings, we'd like
      // /etc/sysconfig/proxy changes to take effect immediately.
      // So we pick the last one matching our schema.
      const std::string myschema { url_r.getScheme()+":" };
      std::optional<std::string> result;
      for ( int i = 0; proxies[i]; ++i ) {
        if ( str::hasPrefix( proxies[i], myschema ) ) {
          result = str::asString( proxies[i] );
          if ( not env::inYAST() )
            break;
        }
      }

      if ( env::PX_DEBUG() ) {
        L_DBG("PX_DEBUG") << "Url " << url_r << endl;
        for ( int i = 0; proxies[i]; ++i ) {
          L_DBG("PX_DEBUG") << "got " << proxies[i] << endl;
        }
        L_DBG("PX_DEBUG") << "--> " << result.value_or( "" ) << endl;
      }

      return result.value_or( "" );
    }

    ProxyInfo::NoProxyIterator ProxyInfoLibproxy::noProxyBegin() const
    { return _no_proxy.begin(); }

    ProxyInfo::NoProxyIterator ProxyInfoLibproxy::noProxyEnd() const
    { return _no_proxy.end(); }

  } // namespace media
} // namespace zypp
