/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "repodownloaderwf.h"
#include <fstream>
#include <utility>

#include <zypp-common/PublicKey.h>
#include <zypp-core/ng/pipelines/mtry.h>
#include <zypp-media/ng/ProvideSpec>

#include <zypp/KeyRing.h>
#include <zypp/ZConfig.h>

#include <zypp/ng/Context>
#include <zypp/ng/media/Provide>
#include <zypp/ng/repo/Downloader>
#include <zypp/ng/repo/workflows/plaindir.h>
#include <zypp/ng/repo/workflows/rpmmd.h>
#include <zypp/ng/repo/workflows/susetags.h>
#include <zypp/ng/reporthelper.h>
#include <zypp/ng/workflows/logichelpers.h>
#include <zypp/ng/workflows/repoinfowf.h>
#include <zypp/ng/workflows/signaturecheckwf.h>

#include <zypp/parser/yum/RepomdFileReader.h>
#include <zypp/repo/RepoException.h>
#include <zypp/repo/RepoMirrorList.h>

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

      using MediaHandle     = typename Provide::MediaHandle;
      using ProvideRes      = typename Provide::Res;

      DownloadMasterIndexLogic( repo::DownloadContextRef &&ctxRef, MediaHandle &&mediaHandle, zypp::filesystem::Pathname &&masterIndex_r  )
        : _dlContext( std::move(ctxRef) )
        , _media(std::move( mediaHandle ))
        , _masterIndex(std::move( masterIndex_r ))
      { }

    public:
      MaybeAsyncRef<expected<repo::DownloadContextRef>> execute( ) {

        zypp::RepoInfo ri = _dlContext->repoInfo();
        // always download them, even if repoGpgCheck is disabled
        _sigpath = _masterIndex.extend( ".asc" );
        _keypath = _masterIndex.extend( ".key" );
        _destdir = _dlContext->destDir();

        auto providerRef = _dlContext->zyppContext()->provider();
          return provider()->provide( _media, _masterIndex, ProvideFileSpec().setDownloadSize( zypp::ByteCount( 20, zypp::ByteCount::MB ) ).setMirrorsAllowed( false ) )
          | and_then( [this]( ProvideRes && masterres ) {
            // update the gpg keys provided by the repo
            return RepoInfoWorkflow::fetchGpgKeys( _dlContext->zyppContext(), _dlContext->repoInfo() )
            | and_then( [this](){

              // fetch signature and maybe key file
              return provider()->provide( _media, _sigpath, ProvideFileSpec().setOptional( true ).setDownloadSize( zypp::ByteCount( 20, zypp::ByteCount::MB ) ).setMirrorsAllowed( false ) )

              | and_then( Provide::copyResultToDest ( provider(), _destdir / _sigpath ) )

              | [this]( expected<zypp::ManagedFile> sigFile ) {
                  zypp::Pathname sigpathLocal { _destdir/_sigpath };
                  if ( !sigFile.is_valid () || !zypp::PathInfo(sigpathLocal).isExist() ) {
                    return makeReadyResult(expected<void>::success()); // no sigfile, valid result
                  }
                  _dlContext->files().push_back( std::move(*sigFile) );

                  // check if we got the key, if not we fall back to downloading the .key file
                  auto expKeyId = mtry( &KeyRing::readSignatureKeyId, _dlContext->zyppContext()->keyRing(), sigpathLocal );
                  if ( expKeyId && !_dlContext->zyppContext()->keyRing()->isKeyKnown(*expKeyId) ) {

                    bool needsMirrorToFetchKey =  _dlContext->repoInfo().baseUrlsEmpty() && _dlContext->repoInfo().mirrorListUrl().isValid() ;
                    if ( needsMirrorToFetchKey ) {
                      // when dealing with mirrors we notify the user to use gpgKeyUrl instead of
                      // fetching the gpg key from any mirror
                      JobReportHelper( _dlContext->zyppContext() ).warning(_("Downloading signature key via mirrors, consider explicitely setting gpgKeyUrl via the repository configuration instead."));
                    }

                    // we did not get the key via gpgUrl downloads, lets fallback
                    return provider()->provide( _media, _keypath, ProvideFileSpec().setOptional( true ).setDownloadSize( zypp::ByteCount( 20, zypp::ByteCount::MB ) ).setMirrorsAllowed(needsMirrorToFetchKey) )
                    | and_then( Provide::copyResultToDest ( provider(), _destdir / _keypath ) )
                    | and_then( [this]( zypp::ManagedFile keyFile ) {
                      _dlContext->files().push_back( std::move(keyFile));
                      return expected<void>::success();
                    });
                  }

                  // we should not reach this line, but if we do we continue and fail later if its required
                  return makeReadyResult(expected<void>::success());
                };
              })
            | [this,masterres=std::move(masterres)]( expected<void> ) {
              return make_expected_success( std::move(masterres) );
            };

          } )
          // execute plugin verification if there is one
          | and_then( std::bind( &DownloadMasterIndexLogic::pluginVerification, this, std::placeholders::_1 ) )

          // signature checking
          | and_then( std::bind( &DownloadMasterIndexLogic::signatureCheck, this, std::placeholders::_1 ) )

          // copy everything into a directory
          | and_then( Provide::copyResultToDest ( providerRef, _destdir / _masterIndex )  )

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

            if ( zypp::PathInfo(sigpathLocal).isExist() && !zypp::PathInfo(keypathLocal).isExist() ) {
              auto kr = _dlContext->zyppContext()->keyRing();
              // if we have a signature but no keyfile, we need to export it from the keyring
              auto expKeyId = mtry( &KeyRing::readSignatureKeyId, kr.get(), sigpathLocal );
              if ( !expKeyId ) {
                MIL << "Failed to read signature from file: " << sigpathLocal << std::endl;
              } else {
                std::ofstream os( keypathLocal.c_str() );
                if ( kr->isKeyKnown(*expKeyId) ) {
                  kr->dumpPublicKey(
                    *expKeyId,
                    kr->isKeyTrusted(*expKeyId),
                    os
                  );
                }
              }
            }

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
                   return providerRef->provide( _media, file, ProvideFileSpec().setOptional(true).setMirrorsAllowed(false) )
                      | and_then( Provide::copyResultToDest( providerRef, _destdir / file ) )
                      | and_then( [this, providerRef, file, keyid , cacheFile = std::move(cacheFile)]( zypp::ManagedFile &&res ) {

                          // remember we downloaded the file
                          _dlContext->files().push_back ( std::move(res) );

                          auto key = zypp::PublicKey::noThrow( _dlContext->files().back() );
                          if ( not key.fileProvidesKey( keyid ) ) {
                            const std::string str = (zypp::str::Str() << "Keyhint " << file << " does not contain a key with id " << keyid << ". Skipping it.");
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

      repo::DownloadContextRef _dlContext;
      MediaHandle _media;
      zypp::Pathname _masterIndex;

      zypp::Pathname _destdir;
      zypp::Pathname _sigpath;
      zypp::Pathname _keypath;
      zypp::TriBool  _repoSigValidated = zypp::indeterminate;

      std::vector<zypp::PublicKeyData> _buddyKeys;
    };

    }

    MaybeAwaitable<expected<repo::DownloadContextRef> > RepoDownloaderWorkflow::downloadMasterIndex(repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle, zypp::Pathname masterIndex_r)
    {
      if constexpr ( ZYPP_IS_ASYNC )
        return SimpleExecutor<DownloadMasterIndexLogic, AsyncOp<expected<repo::DownloadContextRef>>>::run( std::move(dl), std::move(mediaHandle), std::move(masterIndex_r) );
      else
        return SimpleExecutor<DownloadMasterIndexLogic, SyncOp<expected<repo::DownloadContextRef>>>::run( std::move(dl), std::move(mediaHandle), std::move(masterIndex_r) );
    }

    MaybeAwaitable<expected<repo::DownloadContextRef>> RepoDownloaderWorkflow::downloadMasterIndex ( repo::DownloadContextRef dl, LazyMediaHandle<Provide> mediaHandle, zypp::filesystem::Pathname masterIndex_r )
    {
      using namespace zyppng::operators;
      return dl->zyppContext()->provider()->attachMediaIfNeeded( mediaHandle )
      | and_then([ dl, mi = std::move(masterIndex_r) ]( ProvideMediaHandle handle ) mutable {
        return downloadMasterIndex( std::move(dl), std::move(handle), std::move(mi) );
      });
    }


    namespace {
      auto statusImpl ( repo::DownloadContextRef dlCtx, ProvideMediaHandle &&mediaHandle ) {
        const auto finalizeStatus = [ dlCtx ]( zypp::RepoStatus status  ){
           return expected<zypp::RepoStatus>::success( zypp::RepoStatus( dlCtx->repoInfo()) && status );
        };

        switch( dlCtx->repoInfo().type().toEnum()) {
          case zypp::repo::RepoType::RPMMD_e:
            return RpmmdWorkflows::repoStatus( dlCtx, std::forward<ProvideMediaHandle>(mediaHandle) ) | and_then( std::move(finalizeStatus) );
          case zypp::repo::RepoType::YAST2_e:
            return SuseTagsWorkflows::repoStatus( dlCtx, std::forward<ProvideMediaHandle>(mediaHandle) ) | and_then( std::move(finalizeStatus) );
          case zypp::repo::RepoType::RPMPLAINDIR_e:
            return PlaindirWorkflows::repoStatus ( dlCtx, std::forward<ProvideMediaHandle>(mediaHandle) ) | and_then( std::move(finalizeStatus) );
          case zypp::repo::RepoType::NONE_e:
            break;
        }

        return makeReadyResult<expected<zypp::RepoStatus>>( expected<zypp::RepoStatus>::error( ZYPP_EXCPT_PTR (zypp::repo::RepoUnknownTypeException(dlCtx->repoInfo()))) );
      }
    }

    MaybeAwaitable<expected<zypp::RepoStatus> > RepoDownloaderWorkflow::repoStatus(repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle) {
      return statusImpl( dl, std::move(mediaHandle) );
    }

    MaybeAwaitable<expected<zypp::RepoStatus> > RepoDownloaderWorkflow::repoStatus( repo::DownloadContextRef dl, LazyMediaHandle<Provide> mediaHandle ) {
      using namespace zyppng::operators;
      return dl->zyppContext()->provider()->attachMediaIfNeeded( mediaHandle )
      | and_then([ dl ]( ProvideMediaHandle handle ) {
        return repoStatus( dl, std::move(handle) );
      });
    }

    namespace {
      auto downloadImpl ( repo::DownloadContextRef dlCtx, ProvideMediaHandle &&mediaHandle, ProgressObserverRef &&progressObserver ) {
        switch( dlCtx->repoInfo().type().toEnum()) {
          case zypp::repo::RepoType::RPMMD_e:
            return RpmmdWorkflows::download( std::move(dlCtx), std::forward<ProvideMediaHandle>(mediaHandle), std::move(progressObserver) );
          case zypp::repo::RepoType::YAST2_e:
            return SuseTagsWorkflows::download( std::move(dlCtx), std::forward<ProvideMediaHandle>(mediaHandle), std::move(progressObserver) );
          case zypp::repo::RepoType::RPMPLAINDIR_e:
            return PlaindirWorkflows::download ( std::move(dlCtx), std::forward<ProvideMediaHandle>(mediaHandle) );
          case zypp::repo::RepoType::NONE_e:
            break;
        }

        return makeReadyResult<expected<repo::DownloadContextRef> >( expected<repo::DownloadContextRef>::error( ZYPP_EXCPT_PTR (zypp::repo::RepoUnknownTypeException(dlCtx->repoInfo()))) );
      }
    }

    MaybeAwaitable<expected<repo::DownloadContextRef> > RepoDownloaderWorkflow::download(repo::DownloadContextRef dl, ProvideMediaHandle mediaHandle, ProgressObserverRef progressObserver)
    {
      return downloadImpl( dl, std::move(mediaHandle), std::move(progressObserver) );
    }

    MaybeAwaitable<expected<repo::DownloadContextRef> > RepoDownloaderWorkflow::download(repo::DownloadContextRef dl, LazyMediaHandle<Provide> mediaHandle, ProgressObserverRef progressObserver)
    {
      using namespace zyppng::operators;
      return dl->zyppContext()->provider()->attachMediaIfNeeded( mediaHandle )
      | and_then([ dl, po = std::move(progressObserver) ]( ProvideMediaHandle handle ) mutable {
        return downloadImpl( dl, std::move(handle), std::move(po) );
      });
    }
}
