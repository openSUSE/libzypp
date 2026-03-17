/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPPNG_SAT_POOL_H_INCLUDED
#define ZYPPNG_SAT_POOL_H_INCLUDED

#include <zypp-core/base/NonCopyable.h>
#include <zypp-core/Pathname.h>
#include <zypp-core/base/Iterable.h>

#include <zypp/ng/arch.h>
#include <zypp/ng/sat/poolconstants.h>
#include <zypp/ng/sat/components/poolcomponents.h>
#include <zypp/ng/base/serialnumber.h>
#include <zypp/ng/sat/repository.h>
#include <zypp/ng/sat/solvable.h>
#include <zypp/ng/sat/queue.h>
#include <zypp/ng/sat/preparedpool.h>

namespace zyppng::sat {

  /**
   * \class Pool
   * \brief Orchestrator for a libsolv pool instance.
   */
  class Pool
  {
      friend class Repository;
      friend class Solvable;

    public:
      using RepositoryIterable = zypp::Iterable<detail::RepositoryIterator>;
      using SolvableIterable   = zypp::Iterable<detail::SolvableIterator>;

    public:

      /** Default ctor */
      Pool();

      Pool(const Pool &) = delete;
      Pool(Pool &&) = delete;
      Pool &operator=(const Pool &) = delete;
      Pool &operator=(Pool &&) = delete;

      /** Dtor */
      ~Pool();

      /** Expert backdoor. */
      detail::CPool * get() const
      { return _pool; }

      /** Serial number changing whenever the content changes. */
      const SerialNumber & serial() const
      { return _serial; }

      /** Serial number changing whenever resusePoolIDs==true was used. ResPool must also invalidate its PoolItems! */
      const SerialNumber & serialIDs() const
      { return _serialIDs; }

      /** Update housekeeping data (e.g. whatprovides).
       *  Returns a PreparedPool — a move-only view that guarantees the index is valid.
       */
      PreparedPool prepare();


      detail::size_type capacity() const;

      /**
       *  Invalidate everything
       */
      void setDirty( PoolInvalidation invalidation, std::initializer_list<std::string_view> reasons );

      /**
       * \brief Reset the pool by removing all repositories and solvables.
       * \details This is primarily useful for test isolation since StringPool is currently a singleton.
       */
      void clear();

    public:
      /** \name Repository management */
      //@{
      /** Reserved system repository alias \c @System. */
      static const std::string & systemRepoAlias();

      /** Find a \ref Repository named \c alias_r.
       * Returns \ref Repository::noRepository if there is no such \ref Repository.
       */
      Repository reposFind( const std::string & alias_r ) const;

      /** Return a \ref Repository named \c alias_r.
       * It a such a \ref Repository does not already exist
       * a new empty \ref Repository is created.
       */
      Repository reposInsert( const std::string & alias_r );

      /** Remove a \ref Repository named \c alias_r. */
      void reposErase( const std::string & alias_r )
      { reposFind( alias_r ).eraseFromPool(); }

      /** Remove all repos from the pool.
       * This also shrinks a pool which may have become large
       * after having added and removed repos lots of times.
       */
      void reposEraseAll()
      { while ( ! reposEmpty() ) reposErase( repos().begin()->alias() ); }

      /** Whether \ref Pool contains repos. */
      bool reposEmpty() const;

      /** Number of repos in \ref Pool. */
      detail::size_type reposSize() const;

      /** Iteratable to the repositories */
      RepositoryIterable repos() const;

      /** Return the system repository if it is on the pool. */
      Repository findSystemRepo() const;

      /** Return the system repository, create it if missing. */
      Repository systemRepo();

      bool isSystemRepo( detail::CRepo * repo_r ) const
      { return repo_r && _pool->installed == repo_r; }

    private:
      detail::CRepo * _systemRepoPtr() const
      { return _pool->installed; }
      //@}

    public:
      /** \name Solvable management */
      //@{
      /** Whether \ref Pool contains solvables. */
      bool solvablesEmpty() const;

      /** Number of solvables in \ref Pool. */
      detail::size_type solvablesSize() const;

      /** Iterator to the first \ref Solvable. */
      SolvableIterable solvables() const;
      //@}

    public:
      /** \name Component management */
      zypp::Pathname rootDir() const
      {
        const char * rd = ::pool_get_rootdir( _pool );
        return( rd ? rd : "/" );
      }

      /** Set rootdir (for file conflicts check) */
      void rootDir( const zypp::Pathname & root_r )
      {
        if ( root_r.empty() || root_r == "/" )
          ::pool_set_rootdir( _pool, nullptr );
        else
          ::pool_set_rootdir( _pool, root_r.c_str() );
      }

    private:
      /** \name Actions invalidating housekeeping data.
       *
       * All methods expect valid arguments being passed.
       */
      //@{
      /** Creating a new repo named \a name_r. */
      detail::CRepo * _createRepo( const std::string & name_r );

      /** Delete repo \a repo_r from pool. */
      void _deleteRepo( detail::CRepo * repo_r );

      /** Adding solv file to a repo.
       * Except for \c isSystemRepo_r, solvables of incompatible architecture
       * are filtered out.
      */
      int _addSolv( detail::CRepo * repo_r, FILE * file_r );

      /** Adding helix file to a repo.
       * Except for \c isSystemRepo_r, solvables of incompatible architecture
       * are filtered out.
      */
      int _addHelix( detail::CRepo * repo_r, FILE * file_r );

      /** Adding testtags file to a repo.
       * Except for \c isSystemRepo_r, solvables of incompatible architecture
       * are filtered out.
      */
      int _addTesttags( detail::CRepo * repo_r, FILE * file_r );

      /** Adding Solvables to a repo. */
      detail::SolvableIdType _addSolvables( detail::CRepo * repo_r, unsigned count_r );
      //@}

      /** Helper postprocessing the repo after adding solv or helix files. */
      void _postRepoAdd( detail::CRepo * repo_r );

      /** a \c valid \ref Solvable has a non NULL repo pointer. */
      bool validSolvable( const detail::CSolvable & slv_r ) const
      { return slv_r.repo; }
      /** \overload Check also for id_r being in range of _pool->solvables. */
      bool validSolvable( detail::SolvableIdType id_r ) const
      { return id_r < unsigned(_pool->nsolvables) && validSolvable( _pool->solvables[id_r] ); }
      /** \overload Check also for slv_r being in range of _pool->solvables. */
      bool validSolvable( const detail::CSolvable * slv_r ) const
      { return _pool->solvables <= slv_r && slv_r <= _pool->solvables+_pool->nsolvables && validSolvable( *slv_r ); }

      detail::CPool * getPool() const
      { return _pool; }

      /** \todo a quick check whether the repo was meanwhile deleted. */
      detail::CRepo * getRepo( detail::RepoIdType id_r ) const
      { return id_r; }

      /** Return pointer to the sat-solvable or NULL if it is not valid.
       * \see \ref validSolvable.
       */
      detail::CSolvable * getSolvable( detail::SolvableIdType id_r ) const
      {
        if ( validSolvable( id_r ) )
          return &_pool->solvables[id_r];
        return 0;
      }

    public:
      /** libsolv capability parser */
      detail::IdType parserpmrichdep( const char * capstr_r )
      { return ::pool_parserpmrichdep( _pool, capstr_r ); }

      /** Get id of the first valid \ref Solvable.
       * This is the next valid after the system solvable.
       */
      detail::SolvableIdType getFirstId()  const
      { return getNextId( 1 ); }

      /** Get id of the next valid \ref Solvable.
       * This goes round robbin. At the end it returns \ref noSolvableId.
       * Passing \ref noSolvableId it returns the 1st valid  \ref Solvable.
       * \see \ref validSolvable.
       */
      detail::SolvableIdType getNextId( detail::SolvableIdType id_r ) const
      {
        for( ++id_r; id_r < unsigned(_pool->nsolvables); ++id_r )
        {
          if ( validSolvable( _pool->solvables[id_r] ) )
            return id_r;
        }
        return detail::noSolvableId;
      }

      /** Get rootdir (for file conflicts check) */
      //@{
      PoolComponentSet & components() { return _componentsSet; }
      const PoolComponentSet & components() const { return _componentsSet; }

      template <typename T>
      T & component() { return _componentsSet.assertComponent<T>(); }

      template <typename T>
      const T * findComponent() const { return _componentsSet.findComponent<T>(); }
      //@}

    private:
      /** sat-pool. */
      detail::CPool * _pool;
      /** Serial number - changes with each Pool content change. */
      SerialNumber _serial;
      /** Serial number of IDs - changes whenever resusePoolIDs==true - ResPool must also invalidate its PoolItems! */
      SerialNumber _serialIDs;
      /** Watch serial number. */
      SerialNumberWatcher _watcher;
      /** Component set managing modular pool logic. */
      PoolComponentSet _componentsSet;
#ifndef NDEBUG
      /** True while prepare() is running — setDirty() is illegal in this window. */
      bool _preparing = false;
#endif
  };
}

#endif
