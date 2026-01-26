/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/repo/RepoProvideFile.h
 *
*/
#ifndef ZYPP_REPO_REPOPROVIDEFILE_H
#define ZYPP_REPO_REPOPROVIDEFILE_H

#include <iosfwd>

#include <zypp-core/base/PtrTypes.h>
#include <zypp-core/base/Function.h>
#include <zypp/base/Functional.h>
#include <zypp/RepoInfo.h>
#include <zypp/ManagedFile.h>
#include <utility>
#include <zypp-core/OnMediaLocation>
#include <zypp/ProvideFilePolicy.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace repo
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	provideFile
    //
    ///////////////////////////////////////////////////////////////////

    /** Provide a file from a Repository.
     * Let \a source_r provide the file described by \a loc_r. In case
     * \a loc_r contains a checksum, the file is verified. \a policy_r
     * provides callback hooks for download progress reporting and behaviour
     * on failed checksum verification.
     *
     * \throws Exception
    */
    ManagedFile provideFile( RepoInfo repo_r,
                             const OnMediaLocation & loc_r,
                             const ProvideFilePolicy & policy_r = ProvideFilePolicy() );

    /**
     * returns a set of paths.
     *
     * Depending on the privilege level of the effective user, the path requested will be
     * - the user r/w cache location if it's a regular unprivileged user
     * - the system r/o cache location otherwise
     * @param repo_r
     * @return a list of paths
     */
    std::vector<Pathname> repositoryCachePaths( RepoInfo repo_r );

    /**
     * \short Provides files from different repos
     *
     * Class that allows to get files from repositories
     * It handles automatically setting media verifiers if the
     * repo is cached, and reuses media set access opened for
     * repositories during its scope, so you can provide
     * files from different repositories in different order
     * without opening and closing medias all the time
     */
    class ZYPP_API RepoMediaAccess
    {
    public:
      /** Ctor taking the default \ref ProvideFilePolicy. */
      RepoMediaAccess(ProvideFilePolicy defaultPolicy_r = ProvideFilePolicy() );
      ~RepoMediaAccess();

      /** Provide a file from a Repository.
      * Let \a source_r provide the file described by \a loc_r. In case
      * \a loc_r contains a checksum, the file is verified. \a policy_r
      * provides callback hooks for download progress reporting and behaviour
      * on failed checksum verification.
      *
      * \throws Exception
      * \todo Investigate why this needs a non-const Repository as arg.
      */
      ManagedFile provideFile( const RepoInfo& repo_r,
                               const OnMediaLocation & loc_r,
                               const ProvideFilePolicy & policy_r );

      /** \overload Using the current default \ref ProvideFilePolicy. */
      ManagedFile provideFile( RepoInfo repo_r, const OnMediaLocation & loc_r )
      { return provideFile( std::move(repo_r), loc_r, defaultPolicy() ); }

    public:
      /** Set a new default \ref ProvideFilePolicy. */
      void setDefaultPolicy( const ProvideFilePolicy & policy_r );

      /** Get the current default \ref ProvideFilePolicy. */
      const ProvideFilePolicy & defaultPolicy() const;

    public:
      /** Map a resource filename to a local path below a repositories cache.
       *
       * bsc#1253740: Resource filenames pointing to "../" may refer to locations
       * outside the repositories root. That's actually not desired and does not
       * work for mounted media, as they refer to locations outside the mount point.
       * Unfortunately it works for http/ftp and is actually used by TumbleWeed.
       *
       * The remote path relative to the repositories root is usually composed as
       * "repo.path / resource.filename".  Replicating the remote file tree below
       * the repositories cache directory fails if this remote path is relative
       * and refers to "../". It will point to a location outside the cache.
       * To fix this all remote paths will be mapped to filenames below the cache
       * root.
       *
       * So the location of a package on disk should be computed as:
       * \code
       * repo.packagesPath() / RepoMediaAccess::mapToCachePath( repo, resource )
       * \endcode
       */
      static Pathname mapToCachePath( const RepoInfo & repo_r, const OnMediaLocation & resource_r );

   private:
      class Impl;
       RW_pointer<Impl> _impl;
    };

    /////////////////////////////////////////////////////////////////
  } // namespace repo
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_REPO_REPOPROVIDEFILE_H
