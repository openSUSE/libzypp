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

#include <zypp-core/ng/async/AsyncOp>
#include <zypp-core/ng/pipelines/Expected>
#include <functional>

#include <zypp/ng/context.h>
#include <zypp/ng/media/provide.h>

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
    MaybeAwaitable<expected<void>> verifyChecksum (  ContextRef zyppCtx, zypp::CheckSum checksum, zypp::Pathname file );

    /*!
     * Returns a callable that executes the verify checksum as part of a pipeline,
     * forwarding the \ref ProvideRes if the workflow was successful.
     */
    std::function< MaybeAwaitable<expected<ProvideRes>>( ProvideRes && ) > checksumFileChecker( ContextRef zyppCtx, zypp::CheckSum checksum );

  }
}



#endif
