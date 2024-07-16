/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_WORKFLOWS_DOWNLOAD_INCLUDED
#define ZYPP_NG_WORKFLOWS_DOWNLOAD_INCLUDED

#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp-core/ManagedFile.h>
#include <zypp/ng/context_fwd.h>

namespace zyppng {
  ZYPP_FWD_DECL_TYPE_WITH_REFS (ProgressObserver);

  class ProvideMediaHandle;
  class SyncMediaHandle;
  class ProvideFileSpec;

  template<typename ZyppContextRefType>
  class CacheProviderContext : public Base
  {
  protected:
    ZYPP_ADD_PRIVATE_CONSTR_HELPER();
  public:
    using ContextRefType = ZyppContextRefType;
    using ContextType    = typename ZyppContextRefType::element_type;
    using ProvideType    = typename ContextType::ProvideType;
    using MediaHandle    = typename ProvideType::MediaHandle;

    ZYPP_DECL_PRIVATE_CONSTR_ARGS(CacheProviderContext, ZyppContextRefType zyppContext, zypp::Pathname destDir );

    const ContextRefType &zyppContext() const;
    const zypp::Pathname &destDir() const;

    void  addCacheDir( const zypp::Pathname &p );
    const std::vector<zypp::Pathname> &cacheDirs() const;


  protected:
    ContextRefType _zyppContext;
    zypp::Pathname _destDir;
    std::vector<zypp::Pathname> _cacheDirs;
  };

  using AsyncCacheProviderContext = CacheProviderContext<AsyncContextRef>;
  using SyncCacheProviderContext  = CacheProviderContext<SyncContextRef>;
  ZYPP_FWD_DECL_REFS(AsyncCacheProviderContext);
  ZYPP_FWD_DECL_REFS(SyncCacheProviderContext);

  namespace DownloadWorkflow {
    AsyncOpRef<expected<zypp::ManagedFile>> provideToCacheDir( AsyncCacheProviderContextRef cacheContext, ProvideMediaHandle medium, zypp::Pathname file, ProvideFileSpec filespec );
    expected<zypp::ManagedFile> provideToCacheDir( SyncCacheProviderContextRef cacheContext, SyncMediaHandle medium, zypp::Pathname file, ProvideFileSpec filespec );
  }

}


#endif // ZYPP_NG_WORKFLOWS_DOWNLOAD_INCLUDED
