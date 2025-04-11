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
#include <zypp/ng/workflows/contextfacade.h>
#include <zypp-core/ManagedFile.h>
#include <zypp-core/base/String.h>
#include <zypp-core/base/Gettext.h>
#include <zypp-core/fs/TmpPath.h>
#include <zypp/base/StrMatcher.h>
#include <zypp/KeyRing.h>
#include <zypp-common/PublicKey.h>
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

namespace zyppng {

  namespace {

    using namespace zyppng::operators;

    template<class Executor, class OpType>
    struct FetchGpgKeysLogic : public LogicBase<Executor, OpType> {

      using ZyppContextRefType = MaybeAsyncContextRef<OpType>;
      using ZyppContextType = remove_smart_ptr_t<ZyppContextRefType>;
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

        if (  _info.gpgKeyUrlsEmpty() )
          return makeReadyResult( expected<void>::success() );

        _keysDownloaded.clear();

        // no key in the cache is what we are looking for, lets download
        // all keys specified in gpgkey= entries

        // translator: %1% is a repositories name
        _reports.info( zypp::str::Format(_("Looking for gpg keys in repository %1%.") ) % _info.asUserString() );

        return _info.gpgKeyUrls()
         | transform( [this]( const zypp::Url &url ) {

            _reports.info( "  gpgkey=" + url.asString() );
            return _reports.zyppContext()->provider ()->provide( url, zyppng::ProvideFileSpec() )
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
         | [this]( std::list<expected<void>> && ) {
              return expected<void>::success();
            };
      }

    protected:
      JobReportHelper<ZyppContextRefType> _reports;
      const zypp::RepoInfo    _info;
      std::set<std::string> _keysDownloaded;
    };

  }

  expected<void> RepoInfoWorkflow::fetchGpgKeys( SyncContextRef ctx, zypp::RepoInfo info )
  {
    return SimpleExecutor<FetchGpgKeysLogic, SyncOp<expected<void>>>::run( std::move(ctx), std::move(info) );
  }

  AsyncOpRef<expected<void>> RepoInfoWorkflow::fetchGpgKeys(ContextRef ctx, zypp::RepoInfo info )
  {
    return SimpleExecutor<FetchGpgKeysLogic, AsyncOp<expected<void>>>::run( std::move(ctx), std::move(info) );
  }

}
