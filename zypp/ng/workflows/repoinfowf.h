/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_REPOINFOWORKFLOW_INCLUDED
#define ZYPP_NG_REPOINFOWORKFLOW_INCLUDED

#include <zypp-core/Pathname.h>
#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp/RepoInfo.h>
#include <zypp/ng/context_fwd.h>

namespace zyppng {

  namespace RepoInfoWorkflow {
    zypp::Pathname provideKey ( SyncContextRef ctx, RepoInfo info, std::string keyID_r, zypp::Pathname targetDirectory_r );
    AsyncOpRef<zypp::Pathname> provideKey ( AsyncContextRef ctx, RepoInfo info, std::string keyID_r, zypp::Pathname targetDirectory_r );
  }
}


#endif
