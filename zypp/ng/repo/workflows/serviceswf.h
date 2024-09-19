/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_SERVICES_WORKFLOW_INCLUDED
#define ZYPP_NG_SERVICES_WORKFLOW_INCLUDED

#include <zypp-core/zyppng/base/zyppglobal.h>

#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp-core/zyppng/pipelines/Expected>

#include <zypp/RepoManagerFlags.h>
#include <zypp/ng/repomanager.h>

#include <zypp/repo/ServiceType.h>
#include <zypp/ServiceInfo.h>
#include <zypp/ng/context_fwd.h>

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS (ProgressObserver);

  namespace RepoServicesWorkflow {
    AsyncOpRef<expected<std::pair<ServiceInfo, RepoInfoList>>> fetchRepoListfromService( AsyncContextRef ctx, zypp::Pathname root_r, ServiceInfo service, ProgressObserverRef myProgress = nullptr );
    expected<std::pair<ServiceInfo, RepoInfoList>> fetchRepoListfromService( SyncContextRef ctx, zypp::Pathname root_r, ServiceInfo service, ProgressObserverRef myProgress = nullptr );

    AsyncOpRef<expected<zypp::repo::ServiceType> > probeServiceType( AsyncContextRef ctx, const zypp::Url &url );
    expected<zypp::repo::ServiceType> probeServiceType ( SyncContextRef ctx, const zypp::Url &url );

    AsyncOpRef<expected<void>> refreshService ( AsyncRepoManagerRef repoMgr, ServiceInfo info, zypp::RepoManagerFlags::RefreshServiceOptions options );
    expected<void> refreshService ( SyncRepoManagerRef repoMgr, ServiceInfo info, zypp::RepoManagerFlags::RefreshServiceOptions options );
  }
} // namespace zyppng;


#endif
