/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/detail/PoolImpl.h
 *
*/
#ifndef ZYPP_SAT_DETAIL_POOLIMPL_H
#define ZYPP_SAT_DETAIL_POOLIMPL_H
extern "C"
{
#include <solv/pool.h>
#include <solv/repo.h>
#include <solv/solvable.h>
#include <solv/poolarch.h>
#include <solv/repo_solv.h>
#include <solv/pool_parserpmrichdep.h>
}
#include <iosfwd>

#include <zypp/base/Hash.h>
#include <zypp/base/NonCopyable.h>
#include <zypp/base/SerialNumber.h>
#include <zypp/base/SetTracker.h>
#include <zypp/sat/detail/PoolMember.h>
#include <zypp/sat/SolvableSpec.h>
#include <zypp/sat/Queue.h>
#include <zypp/RepoInfo.h>
#include <zypp/Locale.h>
#include <zypp/Capability.h>
#include <zypp/IdString.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace sat
  { /////////////////////////////////////////////////////////////////
    class SolvableSet;
    ///////////////////////////////////////////////////////////////////
    namespace detail
    { /////////////////////////////////////////////////////////////////

      ///////////////////////////////////////////////////////////////////
      //
      //	CLASS NAME : PoolImpl
      //
      /** */
      class PoolImpl : private base::NonCopyable
      {
        public:
          /** Default ctor */
          PoolImpl();

          /** Dtor */
          ~PoolImpl();

          /** Pointer style access forwarded to sat-pool. */
          CPool * operator->()
          { return _pool; }

        public:
          /** Serial number changing whenever the content changes. */
          const SerialNumber & serial() const
          { return _serial; }

          /** Serial number changing whenever resusePoolIDs==true was used. ResPool must also invalidate its PoolItems! */
          const SerialNumber & serialIDs() const
          { return _serialIDs; }

          /** Update housekeeping data (e.g. whatprovides).
           * \todo actually requires a watcher.
           */
          void prepare() const;

        private:
          /** Invalidate housekeeping data (e.g. whatprovides) if the
           *  pools content changed.
           */
          void setDirty( const char * a1 = 0, const char * a2 = 0, const char * a3 = 0 );

          /** Invalidate locale related housekeeping data.
           */
          void localeSetDirty( const char * a1 = 0, const char * a2 = 0, const char * a3 = 0 );

          /** Invalidate housekeeping data (e.g. whatprovides) if dependencies changed.
           */
          void depSetDirty( const char * a1 = 0, const char * a2 = 0, const char * a3 = 0 );

          /** Callback to resolve namespace dependencies (language, modalias, filesystem, etc.). */
          static detail::IdType nsCallback( CPool *, void * data, detail::IdType lhs, detail::IdType rhs );

        public:
          /** Reserved system repository alias \c @System. */
          static const std::string & systemRepoAlias();

          bool isSystemRepo( CRepo * repo_r ) const
          { return repo_r && _pool->installed == repo_r; }

          CRepo * systemRepo() const
          { return _pool->installed; }

          /** Get rootdir (for file conflicts check) */
          Pathname rootDir() const
          {
            const char * rd = ::pool_get_rootdir( _pool );
            return( rd ? rd : "/" );
          }

          /** Set rootdir (for file conflicts check) */
          void rootDir( const Pathname & root_r )
          {
            if ( root_r.empty() || root_r == "/" )
              ::pool_set_rootdir( _pool, nullptr );
            else
              ::pool_set_rootdir( _pool, root_r.c_str() );
          }

        public:
          /** \name Actions invalidating housekeeping data.
           *
           * All methods expect valid arguments being passed.
           */
          //@{
          /** Creating a new repo named \a name_r. */
          CRepo * _createRepo( const std::string & name_r );

          /** Delete repo \a repo_r from pool. */
          void _deleteRepo( CRepo * repo_r );

          /** Adding solv file to a repo.
           * Except for \c isSystemRepo_r, solvables of incompatible architecture
           * are filtered out.
          */
          int _addSolv( CRepo * repo_r, FILE * file_r );

          /** Adding helix file to a repo.
           * Except for \c isSystemRepo_r, solvables of incompatible architecture
           * are filtered out.
          */
          int _addHelix( CRepo * repo_r, FILE * file_r );

          /** Adding testtags file to a repo.
           * Except for \c isSystemRepo_r, solvables of incompatible architecture
           * are filtered out.
          */
          int _addTesttags( CRepo * repo_r, FILE * file_r );

          /** Adding Solvables to a repo. */
          detail::SolvableIdType _addSolvables( CRepo * repo_r, unsigned count_r );
          //@}

          /** Helper postprocessing the repo after adding solv or helix files. */
          void _postRepoAdd( CRepo * repo_r );

        public:
          /** a \c valid \ref Solvable has a non NULL repo pointer. */
          bool validSolvable( const CSolvable & slv_r ) const
          { return slv_r.repo; }
          /** \overload Check also for id_r being in range of _pool->solvables. */
          bool validSolvable( SolvableIdType id_r ) const
          { return id_r < unsigned(_pool->nsolvables) && validSolvable( _pool->solvables[id_r] ); }
          /** \overload Check also for slv_r being in range of _pool->solvables. */
          bool validSolvable( const CSolvable * slv_r ) const
          { return _pool->solvables <= slv_r && slv_r <= _pool->solvables+_pool->nsolvables && validSolvable( *slv_r ); }

        public:
          CPool * getPool() const
          { return _pool; }

          /** \todo a quick check whether the repo was meanwhile deleted. */
          CRepo * getRepo( RepoIdType id_r ) const
          { return id_r; }

          /** Return pointer to the sat-solvable or NULL if it is not valid.
           * \see \ref validSolvable.
           */
          CSolvable * getSolvable( SolvableIdType id_r ) const
          {
            if ( validSolvable( id_r ) )
              return &_pool->solvables[id_r];
            return 0;
          }

        public:
          /** libsolv capability parser */
          sat::detail::IdType parserpmrichdep( const char * capstr_r )
          { return ::pool_parserpmrichdep( _pool, capstr_r ); }

        public:
          /** Get id of the first valid \ref Solvable.
           * This is the next valid after the system solvable.
           */
          SolvableIdType getFirstId()  const
          { return getNextId( 1 ); }

          /** Get id of the next valid \ref Solvable.
           * This goes round robbin. At the end it returns \ref noSolvableId.
           * Passing \ref noSolvableId it returns the 1st valid  \ref Solvable.
           * \see \ref validSolvable.
           */
          SolvableIdType getNextId( SolvableIdType id_r ) const
          {
            for( ++id_r; id_r < unsigned(_pool->nsolvables); ++id_r )
            {
              if ( validSolvable( _pool->solvables[id_r] ) )
                return id_r;
            }
            return noSolvableId;
          }

        public:
          /** */
          const RepoInfo & repoInfo( RepoIdType id_r )
          { return _repoinfos[id_r]; }
          /** Also adjust repo priority and subpriority accordingly. */
          void setRepoInfo( RepoIdType id_r, const RepoInfo & info_r );
          /** */
          void eraseRepoInfo( RepoIdType id_r )
          { _repoinfos.erase( id_r ); }

        public:
          /** Returns the id stored at \c offset_r in the internal
           * whatprovidesdata array.
          */
          const sat::detail::IdType whatProvidesData( unsigned offset_r )
          { return _pool->whatprovidesdata[offset_r]; }

          /** Returns offset into the internal whatprovidesdata array.
           * Use \ref whatProvidesData to get the stored Id.
          */
          unsigned whatProvides( Capability cap_r )
          { prepare(); return ::pool_whatprovides( _pool, cap_r.id() ); }

        public:
          /// \name Requested locales.
          /// The requested LocaleSets managed in _requestedLocalesTracker
          /// are unexpanded; i.e. they contain just the pure user selection.
          /// The resolver however uses expanded sets ('de_DE' will also
          /// include its fallback locales 'de', (en); here in the namespace:
          /// callback and in the Resolver itself).
          //@{
          /** */
          void setTextLocale( const Locale & locale_r );


          /** Start tracking changes based on this \a locales_r.
           * Usually called on TargetInit.
           */
          void initRequestedLocales( const LocaleSet & locales_r );

          /** Added since last initRequestedLocales. */
          const LocaleSet & getAddedRequestedLocales() const
          { return _requestedLocalesTracker.added(); }

          /** Removed since last initRequestedLocales. */
          const LocaleSet & getRemovedRequestedLocales() const
          { return _requestedLocalesTracker.removed(); }

          /** Current set of requested Locales. */
          const LocaleSet & getRequestedLocales() const
          { return _requestedLocalesTracker.current(); }

          bool isRequestedLocale( const Locale & locale_r ) const
          { return _requestedLocalesTracker.contains( locale_r ); }

          /** User change (tracked). */
          void setRequestedLocales( const LocaleSet & locales_r );
          /** User change (tracked). */
          bool addRequestedLocale( const Locale & locale_r );
          /** User change (tracked). */
          bool eraseRequestedLocale( const Locale & locale_r );

          /** All Locales occurring in any repo. */
          const LocaleSet & getAvailableLocales() const;

          bool isAvailableLocale( const Locale & locale_r ) const
          {
            const LocaleSet & avl( getAvailableLocales() );
            LocaleSet::const_iterator it( avl.find( locale_r ) );
            return it != avl.end();
          }

          typedef base::SetTracker<IdStringSet> TrackedLocaleIds;

          /** Expanded _requestedLocalesTracker for solver.*/
          const TrackedLocaleIds & trackedLocaleIds() const;
          //@}

        public:
          /** \name Multiversion install. */
          //@{
          typedef SolvableSet MultiversionList;

          const MultiversionList & multiversionList() const;

          bool isMultiversion( const Solvable & solv_r ) const;

          void multiversionSpecChanged();
          //@}

        public:
          /** \name Installed on behalf of a user request hint. */
          //@{
          /** Get ident list of all autoinstalled solvables. */
          StringQueue autoInstalled() const
          { return _autoinstalled; }

          /** Set ident list of all autoinstalled solvables. */
          void setAutoInstalled( const StringQueue & autoInstalled_r )
          { _autoinstalled = autoInstalled_r; }

          bool isOnSystemByUser( IdString ident_r ) const
          { return !_autoinstalled.contains( ident_r.id() ); }

          bool isOnSystemByAuto( IdString ident_r ) const
          { return _autoinstalled.contains( ident_r.id() ); }
          //@}

        public:
          /** \name Solvables which should trigger the reboot-needed hint if installed/updated. */
          //@{
          /** Set new Solvable specs.*/
          void setNeedrebootSpec( sat::SolvableSpec needrebootSpec_r )
          {
            _needrebootSpec = std::move(needrebootSpec_r);
            _needrebootSpec.setDirty();
          }

          /** Whether \a solv_r matches the spec.*/
          bool isNeedreboot( const Solvable & solv_r ) const
          { return _needrebootSpec.contains( solv_r ); }
          //@}

        public:
          /** \name Blacklisted Solvables. */
          //@{
          bool isRetracted( const Solvable & solv_r ) const
          { return _retractedSpec.contains( solv_r ); }
          bool isPtfMaster( const Solvable & solv_r ) const
          { return _ptfMasterSpec.contains( solv_r ); }
          bool isPtfPackage( const Solvable & solv_r ) const
          { return _ptfPackageSpec.contains( solv_r ); }
          //@}

        public:
          /** accessor for etc/sysconfig/storage reading file on demand */
          const std::set<std::string> & requiredFilesystems() const;

        private:
          /** sat-pool. */
          CPool * _pool;
          /** Serial number - changes with each Pool content change. */
          SerialNumber _serial;
          /** Serial number of IDs - changes whenever resusePoolIDs==true - ResPool must also invalidate its PoolItems! */
          SerialNumber _serialIDs;
          /** Watch serial number. */
          SerialNumberWatcher _watcher;
          /** Additional \ref RepoInfo. */
          std::map<RepoIdType,RepoInfo> _repoinfos;

          /**  */
          base::SetTracker<LocaleSet> _requestedLocalesTracker;
          mutable scoped_ptr<TrackedLocaleIds> _trackedLocaleIdsPtr;

          mutable scoped_ptr<LocaleSet> _availableLocalesPtr;

          /**  */
          void multiversionListInit() const;
          mutable scoped_ptr<MultiversionList> _multiversionListPtr;

          /**  */
          sat::StringQueue _autoinstalled;

          /** Solvables which should trigger the reboot-needed hint if installed/updated. */
          sat::SolvableSpec _needrebootSpec;

          /** Blacklisted specs: */
          sat::SolvableSpec _retractedSpec;
          sat::SolvableSpec _ptfMasterSpec;
          sat::SolvableSpec _ptfPackageSpec;

          /** filesystems mentioned in /etc/sysconfig/storage */
          mutable scoped_ptr<std::set<std::string> > _requiredFilesystemsPtr;
      };
      ///////////////////////////////////////////////////////////////////

      /////////////////////////////////////////////////////////////////
    } // namespace detail
    ///////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////
  } // namespace sat
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#define POOL_SETDIRTY
#endif // ZYPP_SAT_DETAIL_POOLIMPL_H
