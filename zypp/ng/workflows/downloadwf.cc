/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "downloadwf.h"
#include <zypp/ng/workflows/logichelpers.h>

#include <zypp/ng/Context>
#include <zypp/ng/workflows/contextfacade.h>
#include <zypp/ng/workflows/checksumwf.h>


#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/zyppng/pipelines/Algorithm>

#include <zypp-media/ng/Provide>
#include <zypp-media/ng/ProvideSpec>

namespace zyppng {

  template<typename ZyppContextRefType>
  CacheProviderContext<ZyppContextRefType>::CacheProviderContext( ZYPP_PRIVATE_CONSTR_ARG, ZyppContextRefType zyppContext, zypp::Pathname destDir )
    : _zyppContext( std::move(zyppContext) )
    , _destDir( std::move(destDir) )
  { }


  template<typename ZyppContextRefType>
  const typename CacheProviderContext<ZyppContextRefType>::ContextRefType &CacheProviderContext<ZyppContextRefType>::zyppContext() const
  {
    return _zyppContext;
  }


  template<typename ZyppContextRefType>
  const zypp::Pathname &CacheProviderContext<ZyppContextRefType>::destDir() const
  {
    return _destDir;
  }

  template<typename ZyppContextRefType>
  void CacheProviderContext<ZyppContextRefType>::addCacheDir(const zypp::Pathname &p)
  {
    _cacheDirs.push_back ( p );
  }

  template<typename ZyppContextRefType>
  const std::vector<zypp::Pathname> &CacheProviderContext<ZyppContextRefType>::cacheDirs() const
  {
    return _cacheDirs;
  }


  template class CacheProviderContext<ContextRef>;
  template class CacheProviderContext<SyncContextRef>;


  namespace {

    using namespace zyppng::operators;

    class CacheMissException : public zypp::Exception
    {
    public:
      CacheMissException( const zypp::Pathname &filename )
        : zypp::Exception( zypp::str::Str() << filename << " not found in target cache" ) { }
    };

    template <class T>
    struct showme;

    template <class Executor, class OpType>
    struct ProvideFromCacheOrMediumLogic : public LogicBase<Executor, OpType> {
    protected:

      ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);

      using CacheProviderContextRefType = std::conditional_t<detail::is_async_op_v<OpType>, AsyncCacheProviderContextRef, SyncCacheProviderContextRef>;
      using ContextType    = typename remove_smart_ptr_t<CacheProviderContextRefType>::ContextType;
      using ProvideType    = typename ContextType::ProvideType;
      using MediaHandle    = typename ProvideType::MediaHandle;
      using ProvideRes     = typename ProvideType::Res;

    public:
      ProvideFromCacheOrMediumLogic( CacheProviderContextRefType cacheContext, MediaHandle medium, const zypp::Pathname &file, const ProvideFileSpec &filespec )
        : _ctx( std::move(cacheContext) )
        , _medium( std::move(medium) )
        , _file( file )
        , _filespec( filespec ) {}

      MaybeAsyncRef<expected<zypp::ManagedFile>> execute() {

        return findFileInCache( )
        | [this]( expected<zypp::ManagedFile> &&cached ) -> MaybeAsyncRef<expected<zypp::ManagedFile>> {
          if ( !cached ) {
             MIL << "Didn't find " << _file << " in the caches, providing from medium" << std::endl;

             // we didn't find it in the caches or the lookup failed, lets provide and check it
             std::shared_ptr<ProvideType> provider = _ctx->zyppContext()->provider();
             return provider->provide( _medium, _file, _filespec )
             | and_then( [this]( ProvideRes &&res ) {
                return verifyFile( res.file() )
                | and_then( [res = std::move(res)]() {
                  return expected<ProvideRes>::success( std::move(res) );
                });
               })
             | and_then( ProvideType::copyResultToDest( _ctx->zyppContext()->provider(), _ctx->destDir() / _file ) )
             | and_then( []( zypp::ManagedFile &&file ){
                file.resetDispose ();
                return make_expected_success (std::move(file));
              }) ;

          } else {

            return verifyFile ( cached.get() )
            | and_then([ this, cachedFile = std::move(cached.get()) ]() mutable {
                if ( cachedFile == _ctx->destDir() / _file ) {
                  cachedFile.resetDispose(); // make sure dispose is reset
                  return makeReadyResult( expected<zypp::ManagedFile>::success(std::move(cachedFile) ));
                }

                const auto &targetPath = _ctx->destDir() / _file;
                zypp::filesystem::assert_dir( targetPath.dirname () );

                return _ctx->zyppContext()->provider()->copyFile( cachedFile, _ctx->destDir() / _file )
                | and_then( [cachedFile = std::move(cachedFile)]( zypp::ManagedFile &&f) { f.resetDispose(); return make_expected_success (std::move(f)); });
            });
          }
        };
      }

    protected:
      /*!
       * Looks for a specific file in a cache dir, if it exists there and the checksum matches the one in the \sa ProvideFileSpec
       * it is used and returned.
       */
      MaybeAsyncRef<expected<zypp::ManagedFile>> findFileInCache( ) {

        // No checksum - no match
        if ( _filespec.checksum().empty() )
          return makeReadyResult( expected<zypp::ManagedFile>::error(std::make_exception_ptr( CacheMissException(_file) )) );

        const auto &confDirs  = _ctx->cacheDirs();
        const auto targetFile = _ctx->destDir() / _file ;
        std::vector<zypp::Pathname> caches;
        caches.push_back( _ctx->destDir() );
        caches.insert( caches.end(), confDirs.begin(), confDirs.end() );

        auto makeSearchPipeline = [this, targetFile]( zypp::Pathname &&cachePath ){
          zypp::Pathname cacheFilePath( cachePath / _file );
          zypp::PathInfo cacheFileInfo( cacheFilePath );
          if ( !cacheFileInfo.isExist () ) {
            return makeReadyResult(expected<zypp::ManagedFile>::error( std::make_exception_ptr (CacheMissException(_file)) ));
          } else {
            auto provider = _ctx->zyppContext()->provider();

            // calc checksum, but do not use the workflow. Here we don't want to ask the user if a wrong checksum should
            // be accepted
            return provider->checksumForFile( cacheFilePath, _filespec.checksum().type() )
            | and_then([this, cacheFilePath, targetFile]( zypp::CheckSum &&sum ) {

              auto mgdFile = zypp::ManagedFile( cacheFilePath );

              // if the file is in the target dir, make sure to release it if its not used
              if ( cacheFilePath == targetFile )
                mgdFile.setDispose ( zypp::filesystem::unlink );

              if ( sum == _filespec.checksum () ) {
                // we found the file!
                return expected<zypp::ManagedFile>::success( std::move(mgdFile) );
              }

              return expected<zypp::ManagedFile>::error( std::make_exception_ptr (CacheMissException(_file)) );
            });
          }
        };

        auto defVal = expected<zypp::ManagedFile>::error( std::make_exception_ptr (CacheMissException(_file) ) );
        return std::move(caches) | firstOf( std::move(makeSearchPipeline), std::move(defVal), detail::ContinueUntilValidPredicate() );
      }

      MaybeAsyncRef<expected<void>> verifyFile ( const zypp::Pathname &dlFilePath ) {

        return zypp::Pathname( dlFilePath )
        | [this]( zypp::Pathname &&dlFilePath ) {
          if ( !_filespec.checksum().empty () ) {
            return CheckSumWorkflow::verifyChecksum( _ctx->zyppContext(), _filespec.checksum (), dlFilePath );
          }
          return makeReadyResult(expected<void>::success());
        };
        // add other verifier here via and_then(), like a signature based one
      }

      CacheProviderContextRefType _ctx;
      MediaHandle     _medium;
      zypp::Pathname  _file;
      ProvideFileSpec _filespec;
    };
  }

  namespace DownloadWorkflow {

    AsyncOpRef<expected<zypp::ManagedFile> > provideToCacheDir( AsyncCacheProviderContextRef cacheContext, const ProvideMediaHandle &medium, const zypp::Pathname &file, const ProvideFileSpec &filespec )
    {
      return SimpleExecutor<ProvideFromCacheOrMediumLogic, AsyncOp<expected<zypp::ManagedFile>>>::run( std::move(cacheContext), medium, file, filespec );
    }

    expected<zypp::ManagedFile> provideToCacheDir(SyncCacheProviderContextRef cacheContext, const SyncMediaHandle &medium, const zypp::Pathname &file,  const ProvideFileSpec &filespec )
    {
      return SimpleExecutor<ProvideFromCacheOrMediumLogic, SyncOp<expected<zypp::ManagedFile>>>::run( std::move(cacheContext), medium, file, filespec );
    }

  }

}
