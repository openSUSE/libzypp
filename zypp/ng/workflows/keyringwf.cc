/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "keyringwf.h"
#include "logichelpers.h"
#include <zypp/ng/workflows/contextfacade.h>
#include <zypp/zypp_detail/keyring_p.h>
#include <zypp/RepoInfo.h>
#include <zypp/ZConfig.h>
#include <zypp/Pathname.h>
#include <zypp-common/PublicKey.h>

#include <zypp-core/base/Gettext.h>
#include <utility>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp/ng/workflows/repoinfowf.h>
#include <zypp/ng/Context>
#include <zypp/ng/reporthelper.h>
#include <zypp/ng/UserRequest>

namespace zyppng::KeyRingWorkflow {

  template <class Executor, class OpType>
  struct ImportKeyFromRepoLogic : public LogicBase<Executor, OpType> {

    using ZyppContextRefType = MaybeAsyncContextRef<OpType>;
    using ZyppContextType = remove_smart_ptr_t<ZyppContextRefType>;
    using ProvideType     = typename ZyppContextType::ProvideType;
    using MediaHandle     = typename ProvideType::MediaHandle;
    using ProvideRes      = typename ProvideType::Res;

    ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

  public:
    ImportKeyFromRepoLogic( ZyppContextRefType context, std::string &&keyId, zypp::RepoInfo &&info )
      : _context( std::move(context) ), _keyId(std::move(keyId)), _repo( std::move(info) )
    { }

    MaybeAsyncRef<bool> execute () {

      using namespace zyppng::operators;
      using zyppng::operators::operator|;
      using zyppng::expected;

      if ( _keyId.empty() || !_context )
        return makeReadyResult(false);

      const zypp::ZConfig &conf = _context->config();
      zypp::Pathname cacheDir = conf.repoManagerRoot() / conf.pubkeyCachePath();

      return RepoInfoWorkflow::provideKey( _context, _repo, _keyId, cacheDir )
        | [this, cacheDir]( zypp::Pathname myKey ) {
         if ( myKey.empty()  )
           // if we did not find any keys, there is no point in checking again, break
           return false;

         zypp::PublicKey key;
         try {
           key = zypp::PublicKey( myKey );
         } catch ( const zypp::Exception &e ) {
           ZYPP_CAUGHT(e);
           return false;
         }

         if ( !key.isValid() ) {
           ERR << "Key [" << _keyId << "] from cache: " << cacheDir << " is not valid" << std::endl;
           return false;
         }

         MIL << "Key [" << _keyId << "] " << key.name() << " loaded from cache" << std::endl;

         zypp::KeyContext context;
         context.setRepoInfo( _repo );
         if ( !KeyRingReportHelper(_context).askUserToAcceptPackageKey( key, context ) ) {
           return false;
         }

         MIL << "User wants to import key [" << _keyId << "] " << key.name() << " from cache" << std::endl;
         try {
           _context->keyRing()->importKey( key, true );
         } catch ( const zypp::KeyRingException &e ) {
           ZYPP_CAUGHT(e);
           ERR << "Failed to import key: "<<_keyId;
           return false;
         }
         return true;
        };
    }

    ZyppContextRefType _context;
    std::string _keyId;
    zypp::RepoInfo _repo;
  };

  bool provideAndImportKeyFromRepository( SyncContextRef ctx, std::string id_r, zypp::RepoInfo info_r )
  {
    return SimpleExecutor<ImportKeyFromRepoLogic, SyncOp<bool>>::run( ctx, std::move(id_r), std::move(info_r) );
  }

  AsyncOpRef<bool> provideAndImportKeyFromRepository( ContextRef ctx, std::string id_r, zypp::RepoInfo info_r)
  {
    return SimpleExecutor<ImportKeyFromRepoLogic, AsyncOp<bool>>::run( ctx, std::move(id_r), std::move(info_r) );
  }

  namespace {

    /*!
     * \class VerifyFileSignatureLogic
     * Implementation of the logic part for the verifyFileSignature workflow
     */
    template <class Executor, class OpType>
    struct VerifyFileSignatureLogic : public LogicBase<Executor, OpType>
    {
      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

      using ZyppContextRefType = MaybeAsyncContextRef<OpType>;
      using KeyTrust = zypp::KeyRingReport::KeyTrust;

      VerifyFileSignatureLogic( ZyppContextRefType zyppContext, KeyRingRef &&keyRing, zypp::keyring::VerifyFileContext &&ctx )
        : _zyppContext( std::move(zyppContext) )
        , _keyringReport( _zyppContext )
        , _keyRing( std::move(keyRing) )
        , _verifyContext( std::move(ctx) )
      { }

      struct FoundKeyData {
        zypp::PublicKeyData _foundKey;
        zypp::Pathname _whichKeyRing;
        bool trusted = false;
      };

      MaybeAsyncRef<FoundKeyData> findKey ( const std::string &id ) {

        using zyppng::operators::operator|;

        if ( id.empty() )
          return makeReadyResult(FoundKeyData{zypp::PublicKeyData(), zypp::Pathname()});

        // does key exists in trusted keyring
        zypp::PublicKeyData trustedKeyData( _keyRing->pimpl().publicKeyExists( id, _keyRing->pimpl().trustedKeyRing() ) );
        if ( trustedKeyData )
        {
          MIL << "Key is trusted: " << trustedKeyData << std::endl;

          // lets look if there is an updated key in the
          // general keyring
          zypp::PublicKeyData generalKeyData( _keyRing->pimpl().publicKeyExists( id, _keyRing->pimpl().generalKeyRing() ) );
          if ( generalKeyData )
          {
            // bnc #393160: Comment #30: Compare at least the fingerprint
            // in case an attacker created a key the the same id.
            //
            // FIXME: bsc#1008325: For keys using subkeys, we'd actually need
            // to compare the subkey sets, to tell whether a key was updated.
            // because created() remains unchanged if the primary key is not touched.
            // For now we wait until a new subkey signs the data and treat it as a
            //  new key (else part below).
            if ( trustedKeyData.fingerprint() == generalKeyData.fingerprint()
                 && trustedKeyData.created() < generalKeyData.created() )
            {
              MIL << "Key was updated. Saving new version into trusted keyring: " << generalKeyData << std::endl;
              _keyRing->importKey( _keyRing->pimpl().exportKey( generalKeyData, _keyRing->pimpl().generalKeyRing() ), true );
              trustedKeyData = _keyRing->pimpl().publicKeyExists( id, _keyRing->pimpl().trustedKeyRing() ); // re-read: invalidated by import?
            }
          }

          return makeReadyResult( FoundKeyData{ trustedKeyData,  _keyRing->pimpl().trustedKeyRing(), true } );
        }
        else
        {
          zypp::PublicKeyData generalKeyData( _keyRing->pimpl().publicKeyExists( id, _keyRing->pimpl().generalKeyRing() ) );
          if ( generalKeyData )
          {
            zypp::PublicKey key( _keyRing->pimpl().exportKey( generalKeyData, _keyRing->pimpl().generalKeyRing() ) );
            MIL << "Key [" << id << "] " << key.name() << " is not trusted" << std::endl;

            // ok the key is not trusted, ask the user to trust it or not
            zypp::KeyRingReport::KeyTrust reply = _keyringReport.askUserToAcceptKey( key, _verifyContext.keyContext() );
            if ( reply == zypp::KeyRingReport::KEY_TRUST_TEMPORARILY ||
                 reply == zypp::KeyRingReport::KEY_TRUST_AND_IMPORT )
            {
              zypp::Pathname whichKeyring;

              MIL << "User wants to trust key [" << id << "] " << key.name() << std::endl;

              if ( reply == zypp::KeyRingReport::KEY_TRUST_AND_IMPORT )
              {
                MIL << "User wants to import key [" << id << "] " << key.name() << std::endl;
                _keyRing->importKey( key, true );
                whichKeyring = _keyRing->pimpl().trustedKeyRing();
              }
              else
                whichKeyring = _keyRing->pimpl().generalKeyRing();

              return makeReadyResult(FoundKeyData { std::move(generalKeyData), std::move(whichKeyring), true });
            }
            else
            {
              MIL << "User does not want to trust key [" << id << "] " << key.name() << std::endl;
              return makeReadyResult(FoundKeyData { std::move(generalKeyData),  _keyRing->pimpl().generalKeyRing(), false });
            }
          }
          else if ( ! _verifyContext.keyContext().empty() )
          {
            // try to find the key in the repository info
            return provideAndImportKeyFromRepository ( _zyppContext, id, _verifyContext.keyContext().repoInfo() )
              | [this, id]( bool success ) {
                  if ( !success ) {
                    return FoundKeyData{ zypp::PublicKeyData(), zypp::Pathname() };
                  }
                  return FoundKeyData{ _keyRing->pimpl().publicKeyExists( id, _keyRing->pimpl().trustedKeyRing() ),   _keyRing->pimpl().trustedKeyRing(), true };
                };
          }
        }
        return makeReadyResult(FoundKeyData{ zypp::PublicKeyData(), zypp::Pathname() });
      }

      // returns std::pair<bool, zypp::keyring::VerifyFileContext>
      auto execute ()  {

        _verifyContext.resetResults();
        const zypp::Pathname & file         { _verifyContext.file() };
        const zypp::Pathname & signature    { _verifyContext.signature() };
        const std::string & filedesc        { _verifyContext.shortFile() };

        MIL << "Going to verify signature for " << filedesc << " ( " << file << " ) with " << signature << std::endl;

        // if signature does not exists, ask user if they want to accept unsigned file.
        if( signature.empty() || (!zypp::PathInfo( signature ).isExist()) )
        {
          bool res = _keyringReport.askUserToAcceptUnsignedFile( filedesc, _verifyContext.keyContext() );
          MIL << "askUserToAcceptUnsignedFile: " << res << std::endl;
          return makeReadyResult( makeReturn(res) );
        }

        // get the id of the signature (it might be a subkey id!)
        _verifyContext.signatureId( _keyRing->readSignatureKeyId( signature ) ); //throws !
        const std::string & id = _verifyContext.signatureId();

        // collect the buddies
        std::list<zypp::PublicKeyData> buddies;	// Could be imported IFF the file is validated by a trusted key
        for ( const auto & sid : _verifyContext.buddyKeys() ) {
          if ( not zypp::PublicKeyData::isSafeKeyId( sid ) ) {
            WAR << "buddy " << sid << ": key id is too short to safely identify a gpg key. Skipping it." << std::endl;
            continue;
          }
          if ( _keyRing->pimpl().trustedPublicKeyExists( sid ) ) {
            MIL << "buddy " << sid << ": already in trusted key ring. Not needed." << std::endl;
            continue;
          }
          auto pk = _keyRing->pimpl().publicKeyExists( sid );
          if ( not pk ) {
            WAR << "buddy " << sid << ": not available in the public key ring. Skipping it." << std::endl;
            continue;
          }
          if ( pk.providesKey(id) ) {
            MIL << "buddy " << sid << ": is the signing key. Handled separately." << std::endl;
            continue;
          }
          MIL << "buddy " << sid << ": candidate for auto import. Remeber it." << std::endl;
          buddies.push_back( pk );
        }

        using zyppng::operators::operator|;
        return findKey( id ) | [this, id, buddies=std::move(buddies)]( FoundKeyData res ) {

          const zypp::Pathname & file         { _verifyContext.file() };
          const zypp::KeyContext & keyContext { _verifyContext.keyContext() };
          const zypp::Pathname & signature    { _verifyContext.signature() };
          const std::string & filedesc        { _verifyContext.shortFile() };

          if ( res._foundKey ) {

            // we found a key but it is not trusted ( e.g. user did not want to trust it )
            if ( !res.trusted )
              return makeReturn(false);

            // it exists, is trusted, does it validate?
            _verifyContext.signatureIdTrusted( res._whichKeyRing == _keyRing->pimpl().trustedKeyRing() );
            _keyringReport.infoVerify( filedesc, res._foundKey, keyContext );
            if ( _keyRing->pimpl().verifyFile( file, signature, res._whichKeyRing ) )
            {
              _verifyContext.fileValidated( true );
              if ( _verifyContext.signatureIdTrusted() && not buddies.empty() ) {
                // Check for buddy keys to be imported...
                MIL << "Validated with trusted key: importing buddy list..." << std::endl;
                _keyringReport.reportAutoImportKey( buddies, res._foundKey, keyContext );
                for ( const auto & kd : buddies ) {
                  _keyRing->importKey( _keyRing->pimpl().exportKey( kd, _keyRing->pimpl().generalKeyRing() ), true );
                }
              }
              return makeReturn(_verifyContext.fileValidated());	// signature is actually successfully validated!
            }
            else
            {
              bool userAnswer = _keyringReport.askUserToAcceptVerificationFailed( filedesc, _keyRing->pimpl().exportKey( res._foundKey, res._whichKeyRing ), keyContext );
              MIL << "askUserToAcceptVerificationFailed: " << userAnswer << std::endl;
              return makeReturn(userAnswer);
            }
          } else {
            // signed with an unknown key...
            MIL << "File [" << file << "] ( " << filedesc << " ) signed with unknown key [" << id << "]" << std::endl;
            bool res = _keyringReport.askUserToAcceptUnknownKey( filedesc, id, _verifyContext.keyContext() );
            MIL << "askUserToAcceptUnknownKey: " << res << std::endl;
            return makeReturn(res);
          }

          return makeReturn(false);
        };
      }

    protected:
      ZyppContextRefType _zyppContext;
      KeyRingReportHelper<ZyppContextRefType> _keyringReport;
      KeyRingRef _keyRing;
      zypp::keyring::VerifyFileContext _verifyContext;

    private:
      inline std::pair<bool, zypp::keyring::VerifyFileContext> makeReturn( bool res ){
        _verifyContext.fileAccepted( res );
        return std::make_pair( res, std::move(_verifyContext) ) ;
      }
    };
  }

  std::pair<bool,zypp::keyring::VerifyFileContext> verifyFileSignature( SyncContextRef zyppContext, zypp::keyring::VerifyFileContext &&context_r )
  {
    auto kr = zyppContext->keyRing();
    return SimpleExecutor<VerifyFileSignatureLogic, SyncOp<std::pair<bool,zypp::keyring::VerifyFileContext> >>::run( std::move(zyppContext), std::move(kr), std::move(context_r) );
  }

  AsyncOpRef<std::pair<bool,zypp::keyring::VerifyFileContext>> verifyFileSignature( ContextRef zyppContext, zypp::keyring::VerifyFileContext &&context_r )
  {
    auto kr = zyppContext->keyRing();
    return SimpleExecutor<VerifyFileSignatureLogic, AsyncOp<std::pair<bool,zypp::keyring::VerifyFileContext> >>::run( std::move(zyppContext), std::move(kr), std::move(context_r) );
  }

  std::pair<bool,zypp::keyring::VerifyFileContext> verifyFileSignature( SyncContextRef zyppContext, zypp::KeyRing_Ptr keyRing, zypp::keyring::VerifyFileContext &&context_r )
  {
    return SimpleExecutor<VerifyFileSignatureLogic, SyncOp<std::pair<bool,zypp::keyring::VerifyFileContext> >>::run( std::move(zyppContext), std::move(keyRing), std::move(context_r) );
  }

  AsyncOpRef<std::pair<bool,zypp::keyring::VerifyFileContext>> verifyFileSignature(ContextRef zyppContext, zypp::KeyRing_Ptr keyRing, zypp::keyring::VerifyFileContext &&context_r )
  {
    return SimpleExecutor<VerifyFileSignatureLogic, AsyncOp<std::pair<bool,zypp::keyring::VerifyFileContext> >>::run( std::move(zyppContext), std::move(keyRing), std::move(context_r) );
  }

}
