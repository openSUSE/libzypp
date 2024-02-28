/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "signaturecheckwf.h"
#include "keyringwf.h"
#include "logichelpers.h"

#include <zypp/ZYppFactory.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-media/filecheckexception.h>

namespace zyppng {

  namespace {

    using namespace zyppng::operators;

    template <class Executor, class OpType>
    struct VerifySignatureLogic : public LogicBase<Executor,OpType> {

      ZYPP_ENABLE_LOGIC_BASE(Executor,OpType);
      using ZyppContextRefType = MaybeAsyncContextRef<OpType>;

      VerifySignatureLogic( ZyppContextRefType &&zyppCtx, zypp::keyring::VerifyFileContext &&ctx )
        : _zyppCtx( std::move(zyppCtx) )
        , _verifyCtx( std::move(ctx) ) { }

      MaybeAsyncRef<expected<zypp::keyring::VerifyFileContext>> execute () {

        const zypp::Pathname & sig { _verifyCtx.signature() };
        if ( not ( sig.empty() || zypp::PathInfo(sig).isExist() ) ) {
          return makeReadyResult( expected<zypp::keyring::VerifyFileContext>::error(ZYPP_EXCPT_PTR( zypp::SignatureCheckException("Signature " + sig.asString() + " not found.") )));
        }

        MIL << "Checking " << _verifyCtx.file ()<< " file validity using digital signature.." << std::endl;

        return KeyRingWorkflow::verifyFileSignature( _zyppCtx, zypp::keyring::VerifyFileContext( _verifyCtx ) )
         | []( auto &&res  ) {
             if ( not res.first  )
               return expected<zypp::keyring::VerifyFileContext>::error( ZYPP_EXCPT_PTR( zypp::SignatureCheckException( "Signature verification failed for "  + res.second.file().basename() ) ) );
             return expected<zypp::keyring::VerifyFileContext>::success ( std::move( res.second ) );
           };
      }

    protected:
      ZyppContextRefType _zyppCtx;
      zypp::keyring::VerifyFileContext _verifyCtx;
    };
  }

  namespace SignatureFileCheckWorkflow {
    expected<zypp::keyring::VerifyFileContext> verifySignature(SyncContextRef ctx, zypp::keyring::VerifyFileContext &&context )
    {
      return SimpleExecutor<VerifySignatureLogic, SyncOp<expected<zypp::keyring::VerifyFileContext>>>::run( std::move(ctx), std::move(context) );
    }

    AsyncOpRef<expected<zypp::keyring::VerifyFileContext> > verifySignature( ContextRef ctx, zypp::keyring::VerifyFileContext &&context )
    {
      return SimpleExecutor<VerifySignatureLogic, AsyncOp<expected<zypp::keyring::VerifyFileContext>>>::run( std::move(ctx), std::move(context) );
    }
  }
}
