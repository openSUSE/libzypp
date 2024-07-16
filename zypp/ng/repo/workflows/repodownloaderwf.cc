/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "repodownloaderwf.h"
#include <zypp/ng/workflows/logichelpers.h>
#include <zypp/parser/yum/RepomdFileReader.h>
#include <zypp/repo/RepoException.h>

#include <utility>
#include <zypp-media/ng/Provide>
#include <zypp-media/ng/ProvideSpec>
#include <zypp/ng/Context>
#include <zypp/ng/repo/Downloader>
#include <zypp/PublicKey.h>
#include <zypp/KeyRing.h>

#include <zypp/ng/workflows/signaturecheckwf.h>
#include <zypp/ng/repo/workflows/rpmmd.h>
#include <zypp/ng/repo/workflows/susetags.h>
#include <zypp/ng/repo/workflows/plaindir.h>

// sync workflow helpers

#include <zypp/ng/workflows/mediafacade.h>

#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zypp::repomanager"

namespace zyppng {
  namespace {

    using namespace zyppng::operators;

    template < class Executor, class OpType >
    struct DownloadMasterIndexLogic : public LogicBase<Executor, OpType>
    {
    public:
      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

      using DlContextRefType = std::conditional_t<zyppng::detail::is_async_op_v<OpType>, repo::AsyncDownloadContextRef, repo::SyncDownloadContextRef>;
      using ZyppContextType = typename remove_smart_ptr_t<DlContextRefType>::ContextType;
      using ProvideType     = typename ZyppContextType::ProvideType;
      using MediaHandle     = typename ProvideType::MediaHandle;
      using ProvideRes      = typename ProvideType::Res;

      DownloadMasterIndexLogic( DlContextRefType &&ctxRef, MediaHandle &&mediaHandle, zypp::filesystem::Pathname &&masterIndex_r  )
        : _dlContext( std::move(ctxRef) )
        , _media(std::move( mediaHandle ))
        , _masterIndex(std::move( masterIndex_r ))
      { }

    public:
      MaybeAsyncRef<expected<DlContextRefType>> execute( ) {
        // always download them, even if repoGpgCheck is disabled
        _sigpath = _masterIndex.extend( ".asc" );
        _keypath = _masterIndex.extend( ".key" );
        _destdir = _dlContext->destDir();

        auto providerRef = _dlContext->zyppContext()->provider();
        return std::vector {
           // fetch signature and keys
            providerRef->provide( _media, _sigpath, ProvideFileSpec().setOptional( true ).setDownloadSize( zypp::ByteCount( 20, zypp::ByteCount::MB ) ) )
             | and_then( ProvideType::copyResultToDest ( providerRef, _destdir / _sigpath ) ),
            providerRef->provide( _media, _keypath, ProvideFileSpec().setOptional( true ).setDownloadSize( zypp::ByteCount( 20, zypp::ByteCount::MB ) ) )
             | and_then( ProvideType::copyResultToDest ( providerRef, _destdir / _keypath ) ),
           }
          | join()
          | [this]( std::vector<expected<zypp::ManagedFile>> &&res ) {

             // remember downloaded files
             std::for_each( res.begin (), res.end(),
               [this]( expected<zypp::ManagedFile> &f){
                 if (f.is_valid () ) {
                   _dlContext->files().push_back( std::move(f.get()));
                 }
             });

             // get the master index file
             return provider()->provide( _media, _masterIndex, ProvideFileSpec().setDownloadSize( zypp::ByteCount( 20, zypp::ByteCount::MB ) ) );
           }
          // execute plugin verification if there is one
          | and_then( std::bind( &DownloadMasterIndexLogic::pluginVerification, this, std::placeholders::_1 ) )

          // signature checking
          | and_then( std::bind( &DownloadMasterIndexLogic::signatureCheck, this, std::placeholders::_1 ) )

          // copy everything into a directory
          | and_then( ProvideType::copyResultToDest ( providerRef, _destdir / _masterIndex )  )

          // final tasks
          | and_then([this]( zypp::ManagedFile &&masterIndex ) {
             // Accepted!
             _dlContext->repoInfo().setMetadataPath( _destdir );
             _dlContext->repoInfo().setValidRepoSignature( _repoSigValidated );

             // release the media handle
             _media = MediaHandle();
             auto &allFiles = _dlContext->files();

             // make sure the masterIndex is in front
             allFiles.insert( allFiles.begin (), std::move(masterIndex) );
             return make_expected_success( std::move(_dlContext) );
           });
      }


    private:
      auto provider () {
        return _dlContext->zyppContext()->provider();
      }

      MaybeAsyncRef<expected<ProvideRes>> signatureCheck ( ProvideRes &&res ) {

        if ( _dlContext->repoInfo().repoGpgCheck() ) {

          // The local files are in destdir_r, if they were present on the server
          zypp::Pathname sigpathLocal { _destdir/_sigpath };
          zypp::Pathname keypathLocal { _destdir/_keypath };
          bool isSigned = zypp::PathInfo(sigpathLocal).isExist();

          if ( isSigned || _dlContext->repoInfo().repoGpgCheckIsMandatory() ) {

            auto verifyCtx = zypp::keyring::VerifyFileContext( res.file() );

            // only add the signature if it exists
            if ( isSigned )
              verifyCtx.signature( sigpathLocal );

            // only add the key if it exists
            if ( zypp::PathInfo(keypathLocal).isExist() ) {
              try {
                _dlContext->zyppContext()->keyRing()->importKey( zypp::PublicKey(keypathLocal), false );
              } catch (...) {
                return makeReadyResult( expected<ProvideRes>::error( ZYPP_FWD_CURRENT_EXCPT() ) );
              }
            }

            // set the checker context even if the key is not known
            // (unsigned repo, key file missing; bnc #495977)
            verifyCtx.keyContext( _dlContext->repoInfo() );

            return getExtraKeysInRepomd( std::move(res ) )
             | and_then([this, vCtx = std::move(verifyCtx) ]( ProvideRes &&res ) mutable {
                 for ( const auto &keyData : _buddyKeys ) {
                   DBG << "Keyhint remember buddy " << keyData << std::endl;
                   vCtx.addBuddyKey( keyData.id() );
                 }

                 return SignatureFileCheckWorkflow::verifySignature( _dlContext->zyppContext(), std::move(vCtx))
                  | and_then([ this, res = std::move(res) ]( zypp::keyring::VerifyFileContext verRes ){
                    // remember the validation status
                    _repoSigValidated = verRes.fileValidated();
                    return make_expected_success(std::move(res));
                  });
               });

          } else {
            WAR << "Accept unsigned repository because repoGpgCheck is not mandatory for " << _dlContext->repoInfo().alias() << std::endl;
          }
        } else {
          WAR << "Signature checking disabled in config of repository " << _dlContext->repoInfo().alias() << std::endl;
        }
        return makeReadyResult(expected<ProvideRes>::success(res));
      }

      // execute the repo verification if there is one
      expected<ProvideRes> pluginVerification ( ProvideRes &&prevRes ) {
        // The local files are in destdir_r, if they were present on the server
        zypp::Pathname sigpathLocal { _destdir/_sigpath };
        zypp::Pathname keypathLocal { _destdir/_keypath };
        if ( _dlContext->pluginRepoverification() && _dlContext->pluginRepoverification()->isNeeded() ) {
          try {
            _dlContext->pluginRepoverification()->getChecker( sigpathLocal, keypathLocal, _dlContext->repoInfo() )( prevRes.file() );
          } catch ( ... ) {
            return expected<ProvideRes>::error( std::current_exception () );
          }
        }
        return make_expected_success(std::move(prevRes));
      }

      /*!
       * Returns a sync or async expected<ProvideRes> result depending on the
       * implementation class.
       */
      MaybeAsyncRef<expected<ProvideRes>> getExtraKeysInRepomd ( ProvideRes &&res  ) {

        if ( _masterIndex.basename() != "repomd.xml" ) {
          return makeReadyResult( expected<ProvideRes>::success( std::move(res) ) );
        }

        std::vector<std::pair<std::string,std::string>> keyhints { zypp::parser::yum::RepomdFileReader(res.file()).keyhints() };
        if ( keyhints.empty() )
          return makeReadyResult( expected<ProvideRes>::success( std::move(res) ) );
        DBG << "Check keyhints: " << keyhints.size() << std::endl;

        auto keyRing { _dlContext->zyppContext()->keyRing() };
        return zypp::parser::yum::RepomdFileReader(res.file()).keyhints()
          | transform([this, keyRing]( std::pair<std::string, std::string> val ) {

              const auto& [ file, keyid ] = val;
              auto keyData = keyRing->trustedPublicKeyData( keyid );
              if ( keyData ) {
                DBG << "Keyhint is already trusted: " << keyid << " (" << file << ")" << std::endl;
                return makeReadyResult ( expected<zypp::PublicKeyData>::success(keyData) );	// already a trusted key
              }

              DBG << "Keyhint search key " << keyid << " (" << file << ")" << std::endl;

              keyData = keyRing->publicKeyData( keyid );
              if ( keyData )
                return makeReadyResult( expected<zypp::PublicKeyData>::success(keyData) );

              // TODO: Enhance the key caching in general...
              const zypp::ZConfig & conf = _dlContext->zyppContext()->config();
              zypp::Pathname cacheFile = conf.repoManagerRoot() / conf.pubkeyCachePath() / file;

              return zypp::PublicKey::noThrow(cacheFile)
               | [ keyid = keyid ]( auto &&key ){
                   if ( key.fileProvidesKey( keyid ) )
                     return make_expected_success( std::forward<decltype(key)>(key) );
                   else
                     return expected<zypp::PublicKey>::error( std::make_exception_ptr (zypp::Exception("File does not provide key")));
                 }
               | or_else ([ this, file = file, keyid = keyid, cacheFile ] ( auto ) mutable -> MaybeAsyncRef<expected<zypp::PublicKey>> {
                   auto providerRef = _dlContext->zyppContext()->provider();
                   return providerRef->provide( _media, file, ProvideFileSpec().setOptional(true) )
                      | and_then( ProvideType::copyResultToDest( providerRef, _destdir / file ) )
                      | and_then( [this, providerRef, file, keyid , cacheFile = std::move(cacheFile)]( zypp::ManagedFile &&res ) {

                          // remember we downloaded the file
                          _dlContext->files().push_back ( std::move(res) );

                          auto key = zypp::PublicKey::noThrow( _dlContext->files().back() );
                          if ( not key.fileProvidesKey( keyid ) ) {
                            const auto &str = zypp::str::Str() << "Keyhint " << file << " does not contain a key with id " << keyid << ". Skipping it.";
                            WAR << str << std::endl;
                            return makeReadyResult(expected<zypp::PublicKey>::error( std::make_exception_ptr( zypp::Exception(str)) ));
                          }

                          // Try to cache it...
                          zypp::filesystem::assert_dir( cacheFile.dirname() );
                          return providerRef->copyFile( key.path(), cacheFile )
                           | [ key ]( expected<zypp::ManagedFile> res ) mutable {
                               if ( res ) {
                                 // do not delete from cache
                                 res->resetDispose ();
                               }
                               return expected<zypp::PublicKey>::success( std::move(key) );
                             };
                        });
                 })
               | and_then( [ keyRing, keyid = keyid ]( zypp::PublicKey key ){
                   keyRing->importKey( key, false );		// store in general keyring (not trusted!)
                   return expected<zypp::PublicKeyData>::success(keyRing->publicKeyData( keyid ));	// fetch back from keyring in case it was a hidden key
                 });
            })
         | [this, res = res] ( std::vector<expected<zypp::PublicKeyData>> &&keyHints ) mutable {
             std::for_each( keyHints.begin(), keyHints.end(), [this]( expected<zypp::PublicKeyData> &keyData ){
               if ( keyData && *keyData ) {
                 if ( not zypp::PublicKey::isSafeKeyId( keyData->id() ) ) {
                   WAR << "Keyhint " << keyData->id() << " for " << *keyData << " is not strong enough for auto import. Just caching it." << std::endl;
                   return;
                 }
                 _buddyKeys.push_back ( std::move(keyData.get()) );
               }
             });

             MIL << "Check keyhints done. Buddy keys: " << _buddyKeys.size() << std::endl;
             return expected<ProvideRes>::success (std::move(res));
           };
      }

      DlContextRefType _dlContext;
      MediaHandle _media;
      zypp::Pathname _masterIndex;

      zypp::Pathname _destdir;
      zypp::Pathname _sigpath;
      zypp::Pathname _keypath;
      zypp::TriBool  _repoSigValidated = zypp::indeterminate;

      std::vector<zypp::PublicKeyData> _buddyKeys;
    };

    }

    AsyncOpRef<expected<repo::AsyncDownloadContextRef> > RepoDownloaderWorkflow::downloadMasterIndex(repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle, zypp::Pathname masterIndex_r)
    {
      return SimpleExecutor<DownloadMasterIndexLogic, AsyncOp<expected<repo::AsyncDownloadContextRef>>>::run( std::move(dl), std::move(mediaHandle), std::move(masterIndex_r) );
    }

    expected<repo::SyncDownloadContextRef> RepoDownloaderWorkflow::downloadMasterIndex(repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle, zypp::Pathname masterIndex_r)
    {
      return SimpleExecutor<DownloadMasterIndexLogic, SyncOp<expected<repo::SyncDownloadContextRef>>>::run( std::move(dl), std::move(mediaHandle), std::move(masterIndex_r) );
    }

    AsyncOpRef<expected<repo::AsyncDownloadContextRef>> RepoDownloaderWorkflow::downloadMasterIndex ( repo::AsyncDownloadContextRef dl, AsyncLazyMediaHandle mediaHandle, zypp::filesystem::Pathname masterIndex_r )
    {
      using namespace zyppng::operators;
      return dl->zyppContext()->provider()->attachMediaIfNeeded( mediaHandle )
      | and_then([ dl, mi = std::move(masterIndex_r) ]( ProvideMediaHandle handle ) mutable {
        return downloadMasterIndex( std::move(dl), std::move(handle), std::move(mi) );
      });
    }

    expected<repo::SyncDownloadContextRef> RepoDownloaderWorkflow::downloadMasterIndex ( repo::SyncDownloadContextRef dl, SyncLazyMediaHandle mediaHandle, zypp::filesystem::Pathname masterIndex_r )
    {
      using namespace zyppng::operators;
      return dl->zyppContext()->provider()->attachMediaIfNeeded( mediaHandle )
      | and_then([ dl, mi = std::move(masterIndex_r) ]( SyncMediaHandle handle ) mutable {
        return downloadMasterIndex( std::move(dl), std::move(handle), std::move(mi) );
      });
    }


    namespace {
      template <class DlContextRefType, class MediaHandleType>
      auto statusImpl ( DlContextRefType dlCtx, MediaHandleType &&mediaHandle ) {

        constexpr bool isAsync = std::is_same_v<DlContextRefType,repo::AsyncDownloadContextRef>;

        const auto finalizeStatus = [ dlCtx ]( zypp::RepoStatus status  ){
           return expected<zypp::RepoStatus>::success( zypp::RepoStatus( dlCtx->repoInfo()) && status );
        };

        switch( dlCtx->repoInfo().type().toEnum()) {
          case zypp::repo::RepoType::RPMMD_e:
            return RpmmdWorkflows::repoStatus( dlCtx, std::forward<MediaHandleType>(mediaHandle) ) | and_then( std::move(finalizeStatus) );
          case zypp::repo::RepoType::YAST2_e:
            return SuseTagsWorkflows::repoStatus( dlCtx, std::forward<MediaHandleType>(mediaHandle) ) | and_then( std::move(finalizeStatus) );
          case zypp::repo::RepoType::RPMPLAINDIR_e:
            return PlaindirWorkflows::repoStatus ( dlCtx, std::forward<MediaHandleType>(mediaHandle) ) | and_then( std::move(finalizeStatus) );
          case zypp::repo::RepoType::NONE_e:
            break;
        }

        return makeReadyResult<expected<zypp::RepoStatus>, isAsync >( expected<zypp::RepoStatus>::error( ZYPP_EXCPT_PTR (zypp::repo::RepoUnknownTypeException(dlCtx->repoInfo()))) );
      }
    }

    AsyncOpRef<expected<zypp::RepoStatus> > RepoDownloaderWorkflow::repoStatus(repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle) {
      return statusImpl( dl, std::move(mediaHandle) );
    }

    expected<zypp::RepoStatus> RepoDownloaderWorkflow::repoStatus(repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle) {
       return statusImpl( dl, std::move(mediaHandle) );
    }

    AsyncOpRef<expected<zypp::RepoStatus> > RepoDownloaderWorkflow::repoStatus( repo::AsyncDownloadContextRef dl, AsyncLazyMediaHandle mediaHandle ) {
      using namespace zyppng::operators;
      return dl->zyppContext()->provider()->attachMediaIfNeeded( mediaHandle )
      | and_then([ dl ]( ProvideMediaHandle handle ) {
        return repoStatus( dl, std::move(handle) );
      });
    }

    expected<zypp::RepoStatus> RepoDownloaderWorkflow::repoStatus( repo::SyncDownloadContextRef dl, SyncLazyMediaHandle mediaHandle ) {
      using namespace zyppng::operators;
      return dl->zyppContext()->provider()->attachMediaIfNeeded( mediaHandle )
      | and_then([ dl ]( SyncMediaHandle handle ) {
        return repoStatus( dl, std::move(handle) );
      });
    }


    namespace {
      template <class DlContextRefType, class MediaHandleType>
      auto downloadImpl ( DlContextRefType dlCtx, MediaHandleType &&mediaHandle, ProgressObserverRef &&progressObserver ) {

        constexpr bool isAsync = std::is_same_v<DlContextRefType,repo::AsyncDownloadContextRef>;

        switch( dlCtx->repoInfo().type().toEnum()) {
          case zypp::repo::RepoType::RPMMD_e:
            return RpmmdWorkflows::download( std::move(dlCtx), std::forward<MediaHandleType>(mediaHandle), std::move(progressObserver) );
          case zypp::repo::RepoType::YAST2_e:
            return SuseTagsWorkflows::download( std::move(dlCtx), std::forward<MediaHandleType>(mediaHandle), std::move(progressObserver) );
          case zypp::repo::RepoType::RPMPLAINDIR_e:
            return PlaindirWorkflows::download ( std::move(dlCtx), std::forward<MediaHandleType>(mediaHandle) );
          case zypp::repo::RepoType::NONE_e:
            break;
        }

        return makeReadyResult<expected<DlContextRefType>, isAsync >( expected<DlContextRefType>::error( ZYPP_EXCPT_PTR (zypp::repo::RepoUnknownTypeException(dlCtx->repoInfo()))) );
      }
    }

    AsyncOpRef<expected<repo::AsyncDownloadContextRef> > RepoDownloaderWorkflow::download(repo::AsyncDownloadContextRef dl, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver)
    {
      return downloadImpl( dl, std::move(mediaHandle), std::move(progressObserver) );
    }

    expected<repo::SyncDownloadContextRef> RepoDownloaderWorkflow::download(repo::SyncDownloadContextRef dl, SyncMediaHandle mediaHandle, ProgressObserverRef progressObserver)
    {
      return downloadImpl( dl, std::move(mediaHandle), std::move(progressObserver) );
    }

    AsyncOpRef<expected<repo::AsyncDownloadContextRef> > RepoDownloaderWorkflow::download(repo::AsyncDownloadContextRef dl, AsyncLazyMediaHandle mediaHandle, ProgressObserverRef progressObserver)
    {
      using namespace zyppng::operators;
      return dl->zyppContext()->provider()->attachMediaIfNeeded( mediaHandle )
      | and_then([ dl, po = std::move(progressObserver) ]( ProvideMediaHandle handle ) mutable {
        return downloadImpl( dl, std::move(handle), std::move(po) );
      });
    }

    expected<repo::SyncDownloadContextRef> RepoDownloaderWorkflow::download(repo::SyncDownloadContextRef dl, SyncLazyMediaHandle mediaHandle, ProgressObserverRef progressObserver)
    {
      using namespace zyppng::operators;
      return dl->zyppContext()->provider()->attachMediaIfNeeded( mediaHandle )
      | and_then([ dl, po = std::move(progressObserver) ]( SyncMediaHandle handle ) mutable {
        return downloadImpl( dl, std::move(handle), std::move(po) );
      });
    }

}
