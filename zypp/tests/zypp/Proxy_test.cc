#include <boost/test/unit_test.hpp>

#include <zypp-curl/proxyinfo/proxyinfolibproxy.h>
#include <proxy.h>

// compile time check that the libproxy API in the distro we are building against
// is actually what we want
BOOST_AUTO_TEST_CASE(proxy_api)
{
  static_assert( std::is_same_v<decltype(&::px_proxy_factory_new), zypp::media::CreateFactorySig<pxProxyFactory> > );
  static_assert( std::is_same_v<decltype(&::px_proxy_factory_free), zypp::media::DelFactorySig<pxProxyFactory> > );
  static_assert( std::is_same_v<decltype(&::px_proxy_factory_get_proxies), zypp::media::GetProxiesSig<pxProxyFactory> > );
#ifdef LIBPROXY_HAVE_FREE_PROXIES
  static_assert( std::is_same_v<decltype(&::px_proxy_factory_free_proxies), zypp::media::FreeProxiesCb > );
#endif

  // make sure symbols can be resolved
  BOOST_REQUIRE( zypp::media::ProxyInfoLibproxy::isAvailabe() );
}
