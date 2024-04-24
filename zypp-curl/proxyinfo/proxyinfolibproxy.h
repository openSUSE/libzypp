/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/proxyinfo/ProxyInfoLibproxy.h
 *
*/
#ifndef ZYPP_MEDIA_PROXYINFO_PROXYINFOLIBPROXY_H
#define ZYPP_MEDIA_PROXYINFO_PROXYINFOLIBPROXY_H

#include <string>

#include <zypp-core/base/DefaultIntegral>
#include <zypp-curl/ProxyInfo>
#include <zypp-curl/proxyinfo/proxyinfoimpl.h>

extern "C" {
  typedef struct _pxProxyFactory pxProxyFactoryType;
}

namespace zypp {
  namespace media {

    template <typename ProxyFactoryType>
    using CreateFactorySig = ProxyFactoryType *(*) (void);

    template <typename ProxyFactoryType>
    using DelFactorySig    = void (*) (ProxyFactoryType *self);

    template <typename ProxyFactoryType>
    using GetProxiesSig    = char **(*) (ProxyFactoryType *self, const char *url);

    using FreeProxiesCb   = void (*) (char **proxies);

    class ProxyInfoLibproxy : public ProxyInfo::Impl
    {
    public:
      ProxyInfoLibproxy();
      /**  */
      ~ProxyInfoLibproxy() override;

      static bool isAvailabe();

      /**  */
      bool enabled() const override
      { return _enabled; }
      /**  */
      std::string proxy(const Url & url_r) const override;
      /**  */
      ProxyInfo::NoProxyList noProxy() const override
      { return _no_proxy; }
      /**  */
      ProxyInfo::NoProxyIterator noProxyBegin() const override;
      /**  */
      ProxyInfo::NoProxyIterator noProxyEnd() const override;
    private:
      DefaultIntegral<bool,false> _enabled;
      ProxyInfo::NoProxyList _no_proxy;
      pxProxyFactoryType *_factory;
    };

///////////////////////////////////////////////////////////////////

  } // namespace media
} // namespace zypp

#endif // ZYPP_MEDIA_PROXYINFO_PROXYINFOLIBPROXY_H
