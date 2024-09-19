#include <iostream>
#include <boost/test/unit_test.hpp>

#include <zypp/ZYppFactory.h>
#include <zypp/Url.h>
#include <zypp/PathInfo.h>
#include <zypp/TmpPath.h>
#include <zypp/ZConfig.h>

#include <zypp/ng/context.h>
#include <zypp/ng/repo/pluginservices.h>
#include <zypp/ng/serviceinfo.h>

using std::cout;
using std::endl;
using std::string;
using namespace zyppng;
using namespace boost::unit_test;
using namespace zyppng::repo;

#define DATADIR (Pathname(TESTS_SRC_DIR) +  "/repo/yum/data")

class ServiceCollector
{
public:
  typedef std::set<ServiceInfo> ServiceSet;

  ServiceCollector( ServiceSet & services_r )
    : _services( services_r )
  {}

  bool operator()( const ServiceInfo & service_r ) const
  {
    _services.insert( service_r );
    return true;
  }

private:
  ServiceSet & _services;
};


BOOST_AUTO_TEST_CASE(plugin_services)
{
  ServiceCollector::ServiceSet services;

  auto ctx = SyncContext::create();
  ctx->initialize().unwrap();
  PluginServices local(ctx, "/space/tmp/services", ServiceCollector(services));
}

// vim: set ts=2 sts=2 sw=2 ai et:
