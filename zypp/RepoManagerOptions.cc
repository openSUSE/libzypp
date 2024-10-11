/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/RepoManagerOptions.cc
 *
*/

#include "RepoManagerOptions.h"
#include <zypp/ZConfig.h>
#include <zypp/ng/context.h>
#include <zypp/zypp_detail/ZYppImpl.h>

namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  //
  //	class RepoManagerOptions
  //
  ////////////////////////////////////////////////////////////////////

  ZYPP_BEGIN_LEGACY_API
  RepoManagerOptions::RepoManagerOptions( const Pathname & root_r ) : probe(ZConfig::instance().repo_add_probe())
  {
    repoCachePath         = Pathname::assertprefix( root_r, ZConfig::instance().repoCachePath() );
    repoRawCachePath      = Pathname::assertprefix( root_r, ZConfig::instance().repoMetadataPath() );
    repoSolvCachePath     = Pathname::assertprefix( root_r, ZConfig::instance().repoSolvfilesPath() );
    repoPackagesCachePath = Pathname::assertprefix( root_r, ZConfig::instance().repoPackagesPath() );
    knownReposPath        = Pathname::assertprefix( root_r, ZConfig::instance().knownReposPath() );
    knownServicesPath     = Pathname::assertprefix( root_r, ZConfig::instance().knownServicesPath() );
    pluginsPath           = Pathname::assertprefix( root_r, ZConfig::instance().pluginsPath() );


    rootDir = root_r;
    context = zypp_detail::GlobalStateHelper::context();
  }
  ZYPP_END_LEGACY_API

  RepoManagerOptions::RepoManagerOptions( zyppng::ContextBaseRef ctx )
    : probe(ctx->config().repo_add_probe())
    , context(ctx)
  {
    rootDir               = ctx->contextRoot();
    repoCachePath         = Pathname::assertprefix( rootDir, ctx->config().repoCachePath() );
    repoRawCachePath      = Pathname::assertprefix( rootDir, ctx->config().repoMetadataPath() );
    repoSolvCachePath     = Pathname::assertprefix( rootDir, ctx->config().repoSolvfilesPath() );
    repoPackagesCachePath = Pathname::assertprefix( rootDir, ctx->config().repoPackagesPath() );
    knownReposPath        = Pathname::assertprefix( rootDir, ctx->config().knownReposPath() );
    knownServicesPath     = Pathname::assertprefix( rootDir, ctx->config().knownServicesPath() );
    pluginsPath           = Pathname::assertprefix( rootDir, ctx->config().pluginsPath() );
  }

  RepoManagerOptions RepoManagerOptions::makeTestSetup( const Pathname & root_r )
  {
    ZYPP_BEGIN_LEGACY_API
    RepoManagerOptions ret;
    ret.repoCachePath         = root_r;
    ret.repoRawCachePath      = root_r/"raw";
    ret.repoSolvCachePath     = root_r/"solv";
    ret.repoPackagesCachePath = root_r/"packages";
    ret.knownReposPath        = root_r/"repos.d";
    ret.knownServicesPath     = root_r/"services.d";
    ret.pluginsPath           = root_r/"plugins";
    ret.rootDir = root_r;
    ZYPP_END_LEGACY_API
    return ret;
  }

  std::ostream & operator<<( std::ostream & str, const RepoManagerOptions & obj )
  {
#define OUTS(X) str << "  " #X "\t" << obj.X << std::endl
    str << "RepoManagerOptions (" << obj.rootDir << ") {" << std::endl;
    OUTS( repoRawCachePath );
    OUTS( repoSolvCachePath );
    OUTS( repoPackagesCachePath );
    OUTS( knownReposPath );
    OUTS( knownServicesPath );
    OUTS( pluginsPath );
    str << "}" << std::endl;
#undef OUTS
    return str;
  }
}
