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

#include <zypp-core/ng/async/task.h>
#include <zypp-core/ng/base/base.h>
#include <zypp-core/ng/pipelines/Expected>
#include <zypp-core/ManagedFile.h>

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS (Context);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (Provide);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (ProgressObserver);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (CacheProviderContext);

  class ProvideMediaHandle;
  class ProvideFileSpec;

  class CacheProviderContext : public Base
  {
  protected:
    ZYPP_ADD_PRIVATE_CONSTR_HELPER();
  public:

    ZYPP_DECL_PRIVATE_CONSTR_ARGS(CacheProviderContext, ContextRef zyppContext, zypp::Pathname destDir );

    const ContextRef &zyppContext() const;
    const zypp::Pathname &destDir() const;

    void  addCacheDir( const zypp::Pathname &p );
    const std::vector<zypp::Pathname> &cacheDirs() const;


  protected:
    ContextRef _zyppContext;
    zypp::Pathname _destDir;
    std::vector<zypp::Pathname> _cacheDirs;
  };

  namespace DownloadWorkflow {
    MaybeAwaitable<expected<zypp::ManagedFile>> provideToCacheDir( CacheProviderContextRef cacheContext, ProvideMediaHandle medium, zypp::Pathname file, ProvideFileSpec filespec );
  }

}


#endif // ZYPP_NG_WORKFLOWS_DOWNLOAD_INCLUDED
