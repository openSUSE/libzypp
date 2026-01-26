/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/RepoManager.h
 *
*/
#ifndef ZYPP_REPOMANAGER_OPTIONS_H
#define ZYPP_REPOMANAGER_OPTIONS_H

#include <zypp-core/Pathname.h>
#include <zypp-core/Globals.h>
#include <ostream>

namespace zypp
{
  /**
   * Repo manager settings.
   * Settings default to ZYpp global settings.
   */
  struct ZYPP_API RepoManagerOptions
  {
    /** Default ctor following \ref ZConfig global settings.
     * If an optional \c root_r directory is given, all paths  will
     * be prefixed accordingly.
     * \code
     *    root_r\repoCachePath
     *          \repoRawCachePath
     *          \repoSolvCachePath
     *          \repoPackagesCachePath
     *          \knownReposPath
     * \endcode
     */
    RepoManagerOptions( const Pathname & root_r = Pathname() );

    /** Test setup adjusting all paths to be located below one \c root_r directory.
     * \code
     *    root_r\          - repoCachePath
     *          \raw       - repoRawCachePath
     *          \solv      - repoSolvCachePath
     *          \packages  - repoPackagesCachePath
     *          \repos.d   - knownReposPath
     * \endcode
     */
    static RepoManagerOptions makeTestSetup( const Pathname & root_r );

    Pathname repoCachePath;
    Pathname repoRawCachePath;
    Pathname repoSolvCachePath;
    Pathname repoPackagesCachePath;
    Pathname knownReposPath;
    Pathname knownServicesPath;
    Pathname pluginsPath;
    bool probe;
    /**
     * Target distro ID to be used when refreshing repo index services.
     * Repositories not maching this ID will be skipped/removed.
     *
     * If empty, \ref Target::targetDistribution() will be used instead.
     */
    std::string servicesTargetDistro;

    /** remembers root_r value for later use */
    Pathname rootDir;
  };

  std::ostream & operator<<( std::ostream & str, const RepoManagerOptions & obj );
}

#endif
