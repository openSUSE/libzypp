/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "repoinfowf.h"
#include "zypp/ng/reporthelper.h"

#include <zypp-core/ManagedFile.h>
#include <zypp-core/base/String.h>
#include <zypp-core/base/Gettext.h>
#include <zypp-core/fs/TmpPath.h>
#include <zypp/base/StrMatcher.h>
#include <zypp/KeyRing.h>
#include <zypp/PublicKey.h>
#include <zypp/ZYppCallbacks.h>

#include <utility>
#include <zypp-core/zyppng/pipelines/Transform>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp-core/zyppng/pipelines/MTry>
#include <zypp-media/ng/Provide>
#include <zypp-media/ng/ProvideSpec>
#include <zypp/MediaSetAccess.h>
#include <zypp/ng/Context>
#include <zypp/ng/UserRequest>
#include <zypp/ng/workflows/logichelpers.h>

#include <fstream>


namespace zyppng {

  namespace {

    using namespace zyppng::operators;

    template<class Executor, class OpType>
    struct RepoInfoProvideKeyLogic : public LogicBase<Executor, OpType> {

      using ZyppContextRefType = MaybeAsyncContextRef<OpType>;
      using ZyppContextType = remove_smart_ptr_t<ZyppContextRefType>;
      using ProvideType     = typename ZyppContextType::ProvideType;
      using MediaHandle     = typename ProvideType::MediaHandle;
      using ProvideRes      = typename ProvideType::Res;

      RepoInfoProvideKeyLogic( ZyppContextRefType &&zyppContext, zypp::RepoInfo &&info, std::string &&keyID_r, zypp::Pathname &&targetDirectory_r )
        : _reports( std::move(zyppContext ))
        , _info( std::move(info) )
        , _keyID_r(std::move( keyID_r ))
        , _targetDirectory_r(std::move( targetDirectory_r ))
        , _keyIDStr( _keyID_r.size() > 8 ? _keyID_r.substr( _keyID_r.size()-8 ) : _keyID_r )	// print short ID in Jobreports
        , _tempKeyRing( _tmpKeyRingDir )
      { }

      ZYPP_ENABLE_LOGIC_BASE( Executor, OpType );

      MaybeAsyncRef<zypp::filesystem::Pathname> execute () {
        using namespace zyppng::operators;
        using zyppng::operators::operator|;
        using zyppng::expected;


        if ( _keyID_r.empty() )
          return makeReadyResult(zypp::Pathname{});

        importKeysInTargetDir();

        if ( _tempKeyRing.isKeyTrusted( _keyID_r) ) {
          return makeReadyResult(writeKeysToTargetDir());
        }

        if (  _info.gpgKeyUrlsEmpty() ) {
          // translator: %1% is a repositories name
          _reports.info( zypp::str::Format(_("Repository %1% does not define additional 'gpgkey=' URLs.") ) % _info.asUserString() );
          return makeReadyResult(writeKeysToTargetDir());
        }

        // no key in the cache is what we are looking for, lets download
        // all keys specified in gpgkey= entries

        // translator: %1% is a gpg key ID like 3DBDC284
        //             %2% is a repositories name
        _reports.info( zypp::str::Format(_("Looking for gpg key ID %1% in repository %2%.") ) %  _keyIDStr % _info.asUserString() );

        return _info.gpgKeyUrls()
         | transform( [this]( const zypp::Url &url ) {

            _reports.info( "  gpgkey=" + url.asString() );
            return fetchKey( url )
              | and_then( [this, url]( zypp::ManagedFile f ) -> expected<void> {
                  try {
                    if ( f->empty() )
                      return expected<void>::error(std::make_exception_ptr( zypp::Exception("Empty ManagedFile returned.") ));

                    zypp::PublicKey key(f);
                    if ( !key.isValid() )
                      return expected<void>::error(std::make_exception_ptr( zypp::Exception("Invalid public key.") ));

                    // import all keys into our temporary keyring
                    _tempKeyRing.multiKeyImport(f, true);

                  } catch ( const std::exception & e ) {
                    //ignore and continue to next url
                    ZYPP_CAUGHT(e);
                    MIL << "Key import from url:'"<<url<<"' failed." << std::endl;
                    return expected<void>::error( ZYPP_FWD_CURRENT_EXCPT() );
                  }

                  return expected<void>::success();
                });

           })
         | [this]( std::list<expected<void>> && ) ->zypp::Pathname {
              return writeKeysToTargetDir();
            };
      }

    protected:

      MaybeAsyncRef<zyppng::expected<zypp::ManagedFile>> fetchKey ( const zypp::Url &url ) {
        return _reports.zyppContext()->provider ()->provide( url, zyppng::ProvideFileSpec() )
         | and_then( ProvideType::copyResultToDest( _reports.zyppContext()->provider(), _targetDirectory_r / zypp::Pathname( url.getPathName() ).basename() ) );
      }

      void importKeysInTargetDir () {
        MIL << "Check for " << _keyID_r << " at " << _targetDirectory_r << std::endl;

        // translator: %1% is a gpg key ID like 3DBDC284
        //             %2% is a cache directories path
        _reports.info( zypp::str::Format(_("Looking for gpg key ID %1% in cache %2%.") ) %  _keyIDStr %  _targetDirectory_r );
        zypp::filesystem::dirForEach( _targetDirectory_r,
                                      zypp::StrMatcher(".key", zypp::Match::STRINGEND),
                                      [this]( const zypp::Pathname & dir_r, const std::string & str_r ){
            try {

              // deprecate a month old keys
              zypp::PathInfo fileInfo ( dir_r/str_r );
              if ( zypp::Date::now() - fileInfo.mtime() > zypp::Date::month ) {
                //if unlink fails, the file will be overriden in the next step, no need
                //to show a error
                zypp::filesystem::unlink( dir_r/str_r );
              } else {
                _tempKeyRing.multiKeyImport(dir_r/str_r, true);
              }
            } catch (const zypp::KeyRingException& e) {
              ZYPP_CAUGHT(e);
              ERR << "Error importing cached key from file '"<<dir_r/str_r<<"'."<<std::endl;
            }
            return true;
          });
      }

      zypp::Pathname writeKeysToTargetDir() {

        zypp::filesystem::assert_dir(  _targetDirectory_r );

        //now write all keys into their own files in cache, override existing ones to always have
        //up to date key data
        for ( const auto & key: _tempKeyRing.trustedPublicKeyData()) {
          MIL << "KEY ID in KEYRING: " << key.id() << std::endl;

          zypp::Pathname keyFile =  _targetDirectory_r/(zypp::str::Format("%1%.key") % key.rpmName()).asString();

          std::ofstream fout( keyFile.c_str(), std::ios_base::out | std::ios_base::trunc );

          if (!fout)
            ZYPP_THROW(zypp::Exception(zypp::str::form("Cannot open file %s",keyFile.c_str())));

          _tempKeyRing.dumpTrustedPublicKey( key.id(), fout );
        }

        // key is STILL not known, we give up
        if ( !_tempKeyRing.isKeyTrusted( _keyID_r) ) {
          return zypp::Pathname();
        }

        zypp::PublicKeyData keyData( _tempKeyRing.trustedPublicKeyData(  _keyID_r ) );
        if ( !keyData ) {
          ERR << "Error when exporting key from temporary keychain." << std::endl;
          return zypp::Pathname();
        }

        return  _targetDirectory_r/(zypp::str::Format("%1%.key") % keyData.rpmName()).asString();
      }

      JobReportHelper<ZyppContextRefType> _reports;
      const zypp::RepoInfo    _info;
      const std::string _keyID_r;
      const zypp::Pathname    _targetDirectory_r;
      const std::string _keyIDStr;

      zypp::filesystem::TmpDir _tmpKeyRingDir;
      zypp::KeyRing _tempKeyRing;
    };


    struct AsyncRepoInfoProvideKey : public RepoInfoProvideKeyLogic<AsyncRepoInfoProvideKey, zyppng::AsyncOp<zypp::Pathname>>
    {
      using RepoInfoProvideKeyLogic::RepoInfoProvideKeyLogic;
    };

    struct SyncRepoInfoProvideKey : public RepoInfoProvideKeyLogic<SyncRepoInfoProvideKey, zyppng::SyncOp<zypp::Pathname>>
    {
      using RepoInfoProvideKeyLogic::RepoInfoProvideKeyLogic;
    };
  }

  zypp::filesystem::Pathname RepoInfoWorkflow::provideKey( SyncContextRef ctx, zypp::RepoInfo info, std::string keyID_r, zypp::filesystem::Pathname targetDirectory_r )
  {
    return SimpleExecutor<RepoInfoProvideKeyLogic, SyncOp<zypp::filesystem::Pathname>>::run( std::move(ctx), std::move(info), std::move(keyID_r), std::move(targetDirectory_r) );
  }

  AsyncOpRef<zypp::filesystem::Pathname> RepoInfoWorkflow::provideKey(AsyncContextRef ctx, zypp::RepoInfo info, std::string keyID_r, zypp::filesystem::Pathname targetDirectory_r )
  {
    return SimpleExecutor<RepoInfoProvideKeyLogic, AsyncOp<zypp::filesystem::Pathname>>::run( std::move(ctx), std::move(info), std::move(keyID_r), std::move(targetDirectory_r) );
  }

}
