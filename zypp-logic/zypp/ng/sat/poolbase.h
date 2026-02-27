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
#ifndef ZYPPNG_SAT_POOLBASE_H_INCLUDED
#define ZYPPNG_SAT_POOLBASE_H_INCLUDED

#include <zypp-core/base/NonCopyable.h>
#include <zypp-core/Pathname.h>

#include <zypp/ng/arch.h>
#include <zypp/ng/sat/poolconstants.h>
#include <zypp/ng/sat/components/poolcomponents.h>
#include <zypp/ng/base/serialnumber.h>

namespace zyppng::sat {

  class NamespaceRegistry;

  class PoolBase
  {
    public:

      /** Default ctor */
      PoolBase();

      PoolBase(const PoolBase &) = delete;
      PoolBase(PoolBase &&) = delete;
      PoolBase &operator=(const PoolBase &) = delete;
      PoolBase &operator=(PoolBase &&) = delete;

      /** Dtor */
      ~PoolBase();

      /** Pointer style access forwarded to sat-pool. */
      detail::CPool * operator->()
      { return _pool; }

      /** Serial number changing whenever the content changes. */
      const SerialNumber & serial() const
      { return _serial; }

      /** Serial number changing whenever resusePoolIDs==true was used. ResPool must also invalidate its PoolItems! */
      const SerialNumber & serialIDs() const
      { return _serialIDs; }

      /** Update housekeeping data (e.g. whatprovides).
       * \todo actually requires a watcher.
       */
      void prepare();

      /**
       *  Invalidate everything
       */
      void setDirty( PoolInvalidation invalidation, std::initializer_list<std::string_view> reasons );

    public:
      /** Reserved system repository alias \c @System. */
      static const std::string & systemRepoAlias();

      bool isSystemRepo( detail::CRepo * repo_r ) const
      { return repo_r && _pool->installed == repo_r; }

      detail::CRepo * systemRepo() const
      { return _pool->installed; }

      /** Get rootdir (for file conflicts check) */
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

      /** Returns the id stored at \c offset_r in the internal
       * whatprovidesdata array.
      */
      const detail::IdType whatProvidesData( unsigned offset_r )
      { return _pool->whatprovidesdata[offset_r]; }

      /** Returns offset into the internal whatprovidesdata array.
       * Use \ref whatProvidesData to get the stored Id.
      */
      unsigned whatProvidesCapabilityId( detail::IdType cap_r )
      { prepare(); return ::pool_whatprovides( _pool, cap_r ); }

      /** \name Component management */
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
  };
}

#endif
