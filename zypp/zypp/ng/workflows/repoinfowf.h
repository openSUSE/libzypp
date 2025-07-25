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

#include <zypp-core/ng/pipelines/expected.h>
#include <zypp-core/Pathname.h>
#include <zypp-core/ng/pipelines/AsyncResult>
#include <zypp/RepoInfo.h>

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS (Context);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (SyncContext);

  namespace RepoInfoWorkflow {
    /*!
     * Downloads all keys specified by the RepoInfo, importing them into the
     * keyring specified by \a ctx as NON trusted keys.
     */
    expected<void> fetchGpgKeys ( SyncContextRef ctx, zypp::RepoInfo info );
    AsyncOpRef<expected<void>> fetchGpgKeys ( ContextRef ctx, zypp::RepoInfo info );
  }
}


#endif
