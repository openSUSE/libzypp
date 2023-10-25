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

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS (Context);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (SyncContext);

  namespace RepoInfoWorkflow {
    zypp::Pathname provideKey ( SyncContextRef ctx, zypp::RepoInfo info, const std::string &keyID_r, const zypp::Pathname &targetDirectory_r );
    AsyncOpRef<zypp::Pathname> provideKey ( ContextRef ctx, zypp::RepoInfo info, const std::string &keyID_r, const zypp::Pathname &targetDirectory_r );
  }
}


#endif
