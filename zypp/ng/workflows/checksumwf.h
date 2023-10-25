/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_CHECKSUMWORKFLOW_INCLUDED
#define ZYPP_NG_CHECKSUMWORKFLOW_INCLUDED

#include <zypp-core/zyppng/async/AsyncOp>
#include <zypp-core/zyppng/pipelines/Expected>
#include <functional>

#include <zypp/ng/workflows/contextfacade.h>
#include <zypp/ng/workflows/mediafacade.h>

namespace zypp {
  namespace filesystem {
    class Pathname;
  }
  using filesystem::Pathname;
  class CheckSum;
}

namespace zyppng {

  class ProvideRes;
  ZYPP_FWD_DECL_TYPE_WITH_REFS(Context);

  namespace CheckSumWorkflow {
    expected<void> verifyChecksum ( SyncContextRef zyppCtx, const zypp::CheckSum &checksum, const zypp::Pathname &file );
    AsyncOpRef<expected<void>> verifyChecksum (  ContextRef zyppCtx, const zypp::CheckSum &checksum, const zypp::Pathname &file );

    /*!
     * Returns a callable that executes the verify checksum as part of a pipeline,
     * forwarding the \ref ProvideRes if the workflow was successful.
     */
    std::function< AsyncOpRef<expected<ProvideRes>>( ProvideRes && ) > checksumFileChecker( ContextRef zyppCtx, const zypp::CheckSum &checksum );
    std::function< expected<SyncProvideRes>( SyncProvideRes && ) > checksumFileChecker( SyncContextRef zyppCtx, const zypp::CheckSum &checksum );

  }
}



#endif
