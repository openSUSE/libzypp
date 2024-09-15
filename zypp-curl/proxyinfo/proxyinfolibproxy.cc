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

#include <zypp-core/base/Logger.h>
#include <zypp-core/base/String.h>
#include <zypp-core/fs/WatchFile>
#include <zypp-core/Pathname.h>

#include <zypp-curl/proxyinfo/ProxyInfoLibproxy>

#include <dlfcn.h> // for dlload, dlsym, dlclose
#include <glib.h>  // g_clear_pointer and g_strfreev

using std::endl;
using namespace zypp::base;

namespace zypp {
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

    struct TmpUnsetEnv
    {
      TmpUnsetEnv(const char *var_r) : _set(false), _var(var_r) {
        const char * val = getenv( _var.c_str() );
        if ( val )
        {
          _set = true;
          _val = val;
          ::unsetenv( _var.c_str() );
        }
      }

      TmpUnsetEnv(const TmpUnsetEnv &) = delete;
      TmpUnsetEnv(TmpUnsetEnv &&) = delete;
      TmpUnsetEnv &operator=(const TmpUnsetEnv &) = delete;
      TmpUnsetEnv &operator=(TmpUnsetEnv &&) = delete;

      ~TmpUnsetEnv()
      {
        if ( _set )
        {
          setenv( _var.c_str(), _val.c_str(), 1 );
        }
      }

      bool _set;
      std::string _var;
      std::string _val;
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

      /* cURL can only handle HTTP proxies, not SOCKS. And can only handle
         one. So look through the list and find an appropriate one. */
      std::optional<std::string> result;
      for (int i = 0; proxies[i]; i++) {
        if ( !result && !strncmp(proxies[i], "http://", 7) ) {
          result = str::asString( proxies[i] );
        }
      }

      return result.value_or( "" );
    }

    ProxyInfo::NoProxyIterator ProxyInfoLibproxy::noProxyBegin() const
    { return _no_proxy.begin(); }

    ProxyInfo::NoProxyIterator ProxyInfoLibproxy::noProxyEnd() const
    { return _no_proxy.end(); }

  } // namespace media
} // namespace zypp
