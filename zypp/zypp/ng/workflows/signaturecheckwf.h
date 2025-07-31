/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_SIGNATURECHECK_WORKFLOW_INCLUDED
#define ZYPP_NG_SIGNATURECHECK_WORKFLOW_INCLUDED

#include <zypp-core/ng/pipelines/AsyncResult>
#include <zypp-core/ng/pipelines/Expected>

#include <zypp-common/PublicKey.h>
#include <zypp/Pathname.h>
#include <zypp/KeyRingContexts.h>

// async helper
#include <zypp/ng/workflows/contextfacade.h>

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS (Context);

  namespace SignatureFileCheckWorkflow {
    expected<zypp::keyring::VerifyFileContext> verifySignature( SyncContextRef ctx, zypp::keyring::VerifyFileContext context );
    AsyncOpRef<expected<zypp::keyring::VerifyFileContext>> verifySignature( ContextRef ctx, zypp::keyring::VerifyFileContext context );
  }

}



#endif
