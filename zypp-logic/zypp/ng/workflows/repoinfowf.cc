/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "repoinfowf.h"
#include <zypp/ng/reporthelper.h>
#include <zypp/repo/RepoMirrorList.h>
#include <zypp-core/ManagedFile.h>
#include <zypp-core/base/String.h>
#include <zypp-core/base/Gettext.h>
#include <zypp-core/fs/TmpPath.h>
#include <zypp/base/StrMatcher.h>
#include <zypp/KeyRing.h>
#include <zypp-common/PublicKey.h>
#include <zypp/ZYppCallbacks.h>

#include <utility>
#include <zypp-core/ng/pipelines/Transform>
#include <zypp-core/ng/pipelines/Expected>
#include <zypp-core/ng/pipelines/MTry>
#include <zypp/ng/media/provide.h>
#include <zypp-media/ng/ProvideSpec>
#include <zypp/ng/Context>
#include <zypp/ng/UserRequest>
#include <zypp/ng/workflows/logichelpers.h>

namespace zyppng {

  namespace {

    using namespace zyppng::operators;

    template<class Executor, class OpType>
    struct FetchGpgKeysLogic : public LogicBase<Executor, OpType> {

      using ZyppContextRefType =ContextRef;
      using ZyppContextType = Context;
      using ProvideType     = typename ZyppContextType::ProvideType;
      using MediaHandle     = typename ProvideType::MediaHandle;
      using ProvideRes      = typename ProvideType::Res;

      FetchGpgKeysLogic( ZyppContextRefType &&zyppContext, zypp::RepoInfo &&info )
        : _reports( std::move(zyppContext ))
        , _info( std::move(info) )
      { }

      ZYPP_ENABLE_LOGIC_BASE( Executor, OpType );

      MaybeAsyncRef<expected<void>> execute () {
        using namespace zyppng::operators;
        using zyppng::operators::operator|;
        using zyppng::expected;

        zypp::RepoInfo::url_set gpgKeyUrls = _info.gpgKeyUrls();

        if (  gpgKeyUrls.empty() ) {
          if ( !_info.baseUrlsEmpty()
               && zypp::repo::RepoMirrorList::urlSupportsMirrorLink(*_info.baseUrlsBegin()) ) {

            MIL << "No gpgkey URL specified, but d.o.o server detected. Trying to generate the key file path." << std::endl;

            zypp::Url bUrl = *_info.baseUrlsBegin();
            zypp::repo::RepoType::Type rType = _info.type().toEnum ();
            switch( rType ) {
             case zypp::repo::RepoType::RPMMD_e:
                bUrl.appendPathName( _info.path() / "/repodata/repomd.xml.key" );
                gpgKeyUrls.push_back( bUrl );
                break;
              case zypp::repo::RepoType::YAST2_e:
                bUrl.appendPathName( _info.path() / "/content.key" );
                gpgKeyUrls.push_back( bUrl );
                break;
              case zypp::repo::RepoType::NONE_e:
              case zypp::repo::RepoType::RPMPLAINDIR_e: {
                MIL << "Repo type is not known, unable to generate the gpgkey Url on the fly." << std::endl;
                break;
              }
            }
          }

          if ( gpgKeyUrls.empty () )
            return makeReadyResult( expected<void>::success() );
        }

        _keysDownloaded.clear();

        // no key in the cache is what we are looking for, lets download
        // all keys specified in gpgkey= entries

        // translator: %1% is a repositories name
        _reports.info( zypp::str::Format(_("Looking for gpg keys in repository %1%.") ) % _info.asUserString() );

        return std::move(gpgKeyUrls)
         | transform( [this]( const zypp::Url &url ) {

            _reports.info( "  gpgkey=" + url.asString() );
            return _reports.zyppContext()->provider ()->provide( url, zyppng::ProvideFileSpec().setMirrorsAllowed(false) )
              | and_then( [this, url]( ProvideRes f ) -> expected<void> {
                  try {
                    zypp::PublicKey key(f.file());
                    if ( !key.isValid() )
                      return expected<void>::error(std::make_exception_ptr( zypp::Exception("Invalid public key.") ));

                    // import all keys into our keyring
                    _reports.zyppContext()->keyRing()->multiKeyImport(f.file(), false);

                  } catch ( const std::exception & e ) {
                    //ignore and continue to next url
                    ZYPP_CAUGHT(e);
                    MIL << "Key import from url:'"<<url<<"' failed." << std::endl;
                    return expected<void>::error( ZYPP_FWD_CURRENT_EXCPT() );
                  }

                  return expected<void>::success();
                });
           } )
         | []( std::list<expected<void>> && ) {
              return expected<void>::success();
            };
      }

    protected:
      JobReportHelper _reports;
      const zypp::RepoInfo    _info;
      std::set<std::string> _keysDownloaded;
    };

  }

  MaybeAwaitable<expected<void>> RepoInfoWorkflow::fetchGpgKeys(ContextRef ctx, zypp::RepoInfo info )
  {
    if constexpr ( ZYPP_IS_ASYNC )
      return SimpleExecutor<FetchGpgKeysLogic, AsyncOp<expected<void>>>::run( std::move(ctx), std::move(info) );
    else
      return SimpleExecutor<FetchGpgKeysLogic, SyncOp<expected<void>>>::run( std::move(ctx), std::move(info) );
  }

}
