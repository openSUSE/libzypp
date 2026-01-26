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

#include <zypp-core/ng/base/zyppglobal.h>

#include <zypp-core/ng/async/task.h>
#include <zypp-core/ng/pipelines/Expected>

#include <zypp/RepoManagerFlags.h>
#include <zypp/ng/repomanager.h>

#include <zypp/repo/ServiceType.h>
#include <zypp/ServiceInfo.h>


namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS (Context);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (ProgressObserver);

  namespace RepoServicesWorkflow {
    MaybeAwaitable<expected<std::pair<zypp::ServiceInfo, RepoInfoList>>> fetchRepoListfromService( ContextRef ctx, zypp::Pathname root_r, ServiceInfo service, ProgressObserverRef myProgress = nullptr );

    MaybeAwaitable<expected<zypp::repo::ServiceType> > probeServiceType( ContextRef ctx, const zypp::Url &url );

    MaybeAwaitable<expected<void>> refreshService ( RepoManagerRef repoMgr, zypp::ServiceInfo info, zypp::RepoManagerFlags::RefreshServiceOptions options );
  }
} // namespace zyppng;


#endif
