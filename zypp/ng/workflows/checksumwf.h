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

#include <zypp/ng/context_fwd.h>
#include <zypp-core/zyppng/async/AsyncOp>
#include <zypp-core/zyppng/pipelines/Expected>
#include <functional>


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

  ZYPP_FWD_DECL_TYPE_WITH_REFS (ProgressObserver);

  namespace CheckSumWorkflow {
    expected<void> verifyChecksum (SyncContextRef zyppCtx, zyppng::ProgressObserverRef taskObserver, zypp::CheckSum checksum, zypp::Pathname file );
    AsyncOpRef<expected<void>> verifyChecksum (  AsyncContextRef zyppCtx, ProgressObserverRef taskObserver, zypp::CheckSum checksum, zypp::Pathname file );

    /*!
     * Returns a callable that executes the verify checksum as part of a pipeline,
     * forwarding the \ref ProvideRes if the workflow was successful.
     */
    std::function< AsyncOpRef<expected<ProvideRes>>( ProvideRes && ) > checksumFileChecker(AsyncContextRef zyppCtx, zyppng::ProgressObserverRef taskObserver, zypp::CheckSum checksum );
    std::function< expected<SyncProvideRes>( SyncProvideRes && ) > checksumFileChecker( SyncContextRef zyppCtx, ProgressObserverRef taskObserver, zypp::CheckSum checksum );

  }
}



#endif
