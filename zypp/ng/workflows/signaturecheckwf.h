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

#include <zypp/ng/context_fwd.h>
#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp-core/zyppng/pipelines/Expected>

#include <zypp/PublicKey.h>
#include <zypp/Pathname.h>
#include <zypp/KeyRingContexts.h>

// async helper


namespace zyppng {

  namespace SignatureFileCheckWorkflow {
    expected<zypp::keyring::VerifyFileContext> verifySignature( SyncContextRef ctx, zypp::keyring::VerifyFileContext context );
    AsyncOpRef<expected<zypp::keyring::VerifyFileContext>> verifySignature( AsyncContextRef ctx, zypp::keyring::VerifyFileContext context );
  }

}



#endif
