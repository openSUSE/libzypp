/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-curl/proxyinfo/ProxyInfoSysconfig
 *
*/
#ifndef ZYPP_CURL_PROXYINFO_PROXYINFOSYSCONFIG_H_INCLUDED
#define ZYPP_CURL_PROXYINFO_PROXYINFOSYSCONFIG_H_INCLUDED

#include <string>
#include <map>

#include <zypp-core/parser/Sysconfig>
#include <zypp-core/base/DefaultIntegral>
#include <zypp-curl/ProxyInfo>
#include <zypp-curl/proxyinfo/proxyinfoimpl.h>

namespace zypp {
  namespace media {


    class ProxyInfoSysconfig : public ProxyInfo::Impl
    {
    public:
      ProxyInfoSysconfig(const Pathname & path);
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
      std::map<std::string,std::string> _proxies;
    };

///////////////////////////////////////////////////////////////////

  } // namespace media
} // namespace zypp

#endif // ZYPP_CURL_PROXYINFO_PROXYINFOSYSCONFIG_H_INCLUDED
