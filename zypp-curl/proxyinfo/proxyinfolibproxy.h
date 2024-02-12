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
#include <map>

#include <proxy.h>

#include <zypp-core/base/DefaultIntegral>
#include <zypp-curl/ProxyInfo>
#include <zypp-curl/proxyinfo/proxyinfoimpl.h>

namespace zypp {
  namespace media {


    class ProxyInfoLibproxy : public ProxyInfo::Impl
    {
    public:
      ProxyInfoLibproxy();
      /**  */
      ~ProxyInfoLibproxy() override;
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
      pxProxyFactory *_factory;
    };

///////////////////////////////////////////////////////////////////

  } // namespace media
} // namespace zypp

#endif // ZYPP_MEDIA_PROXYINFO_PROXYINFOLIBPROXY_H
