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

namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  //
  //	class RepoManagerOptions
  //
  ////////////////////////////////////////////////////////////////////

  RepoManagerOptions::RepoManagerOptions( const Pathname & root_r )
  {
    repoCachePath         = Pathname::assertprefix( root_r, ZConfig::instance().repoCachePath() );
    repoRawCachePath      = Pathname::assertprefix( root_r, ZConfig::instance().repoMetadataPath() );
    repoSolvCachePath     = Pathname::assertprefix( root_r, ZConfig::instance().repoSolvfilesPath() );
    repoPackagesCachePath = Pathname::assertprefix( root_r, ZConfig::instance().repoPackagesPath() );
    knownReposPath        = Pathname::assertprefix( root_r, ZConfig::instance().knownReposPath() );
    knownServicesPath     = Pathname::assertprefix( root_r, ZConfig::instance().knownServicesPath() );
    pluginsPath           = Pathname::assertprefix( root_r, ZConfig::instance().pluginsPath() );
    probe                 = ZConfig::instance().repo_add_probe();

    rootDir = root_r;
  }

  RepoManagerOptions RepoManagerOptions::makeTestSetup( const Pathname & root_r )
  {
    RepoManagerOptions ret;
    ret.repoCachePath         = root_r;
    ret.repoRawCachePath      = root_r/"raw";
    ret.repoSolvCachePath     = root_r/"solv";
    ret.repoPackagesCachePath = root_r/"packages";
    ret.knownReposPath        = root_r/"repos.d";
    ret.knownServicesPath     = root_r/"services.d";
    ret.pluginsPath           = root_r/"plugins";
    ret.rootDir = root_r;
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
