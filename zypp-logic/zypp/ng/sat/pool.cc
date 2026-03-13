/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "pool.h"
#include "stringpool.h"


#include <zypp-core/base/LogTools.h>
#include <zypp/ng/sat/capability.h>


extern "C"
{
// Workaround libsolv project not providing a common include
// directory. (the -devel package does, but the git repo doesn't).
// #include <solv/repo_helix.h>
// #include <solv/testcase.h>
int repo_add_helix( ::Repo *repo, FILE *fp, int flags );
int testcase_add_testtags(Repo *repo, FILE *fp, int flags);
}


#undef  ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zyppng::satpool"


namespace zyppng::sat {

  namespace env {
    inline int LIBSOLV_DEBUGMASK() {
      const char * envp = getenv("LIBSOLV_DEBUGMASK");
      return envp ? zypp::str::strtonum<int>( envp ) : 0;
    }
  } // namespace env


  namespace detail {
    // Compile-time checks for satlib constants we redefine to avoid
    // includes and defines.
    static_assert( noId                 == STRID_NULL );
    static_assert( emptyId              == STRID_EMPTY );

    static_assert( noSolvableId         == ID_NULL );
    static_assert( systemSolvableId     == SYSTEMSOLVABLE );

    static_assert( solvablePrereqMarker == SOLVABLE_PREREQMARKER );
    static_assert( solvableFileMarker   == SOLVABLE_FILEMARKER );

    static_assert( Capability::CAP_AND       == REL_AND );
    static_assert( Capability::CAP_OR        == REL_OR );
    static_assert( Capability::CAP_COND      == REL_COND );
    static_assert( Capability::CAP_UNLESS    == REL_UNLESS );
    static_assert( Capability::CAP_ELSE      == REL_ELSE );
    static_assert( Capability::CAP_WITH      == REL_WITH );
    static_assert( Capability::CAP_WITHOUT   == REL_WITHOUT );
    static_assert( Capability::CAP_NAMESPACE == REL_NAMESPACE );
    static_assert( Capability::CAP_ARCH      == REL_ARCH );

    static_assert( namespaceModalias    == NAMESPACE_MODALIAS );
    static_assert( namespaceLanguage    == NAMESPACE_LANGUAGE );
    static_assert( namespaceFilesystem  == NAMESPACE_FILESYSTEM );


    static void logSat( CPool *, void *data, int type, const char *logString )
    {
      //                            "1234567890123456789012345678901234567890
      if ( 0 == strncmp( logString, "job: drop orphaned", 18 ) )
        return;
      if ( 0 == strncmp( logString, "job: user installed", 19 ) )
        return;
      if ( 0 == strncmp( logString, "job: multiversion", 17 ) )
        return;
      if ( 0 == strncmp( logString, "  - no rule created", 19 ) )
        return;
      if ( 0 == strncmp( logString, "    next rules: 0 0", 19 ) )
        return;

      if ( type & (SOLV_FATAL|SOLV_ERROR) ) {
        L_ERR("libsolv") << logString;
      } else if ( type & SOLV_DEBUG_STATS ) {
        L_DBG("libsolv") << logString;
      } else {
        L_MIL("libsolv") << logString;
      }
    }
  }

  Pool::Pool()
    : _pool( StringPool::instance().getPool() )
  {
  }

  Pool::~Pool()
  {

  }

  detail::size_type Pool::capacity() const
  { return _pool->nsolvables; }

  void Pool::prepare()
  {
    // The Stability Loop: loop as long as the serial number is changing.
    // This catches invalidations triggered by components during their preparation
    // and ensures all components see a consistent state.
    int safety_break = 10;
    while ( _watcher.remember( _serial ) && safety_break-- > 0 )
    {
      _componentsSet.notifyPrepare( *this );

      // Legacy/Fallback logic for things not yet componentized:
      if ( ! _pool->whatprovides ) {
        MIL << "pool_createwhatprovides..." << std::endl;
        ::pool_addfileprovides( _pool );
        ::pool_createwhatprovides( _pool );
      }
    }

    if ( safety_break <= 0 ) {
      L_ERR("zyppng::satpool") << "Pool failed to stabilize after 10 prepare iterations!";
    }
  }

  void Pool::clear()
  {
    ::pool_freeallrepos( _pool, /*resusePoolIDs*/true );
    _serialIDs.setDirty();
    setDirty( PoolInvalidation::Data, { __FUNCTION__ } );
  }

  void Pool::setDirty( PoolInvalidation invalidation, std::initializer_list<std::string_view> reasons )
  {
      if ( reasons.size() ) {
        bool first = true;
        for ( std::string_view r : reasons ) {
          if ( first )
            MIL << r;
          else
            MIL << " " << r;
          first = false;
        }
        MIL << std::endl;
      }

      // Notify components so they can clear their caches
      _componentsSet.notifyInvalidate( *this, invalidation );

      if ( invalidation == PoolInvalidation::Data )
        _serial.setDirty ();

      ::pool_freewhatprovides( _pool );
  }

  const std::string &Pool::systemRepoAlias()
  {
    static const std::string _val( "@System" );
    return _val;
  }

  Repository Pool::reposFind( const std::string & alias_r ) const
  {
    for( const auto &repo : repos() )
    {
      if ( alias_r == repo.alias() )
        return repo;
    }
    return Repository();
  }

  Repository Pool::reposInsert( const std::string & alias_r )
  {
    Repository res( reposFind( alias_r ) );
    if ( ! res )
      res = Repository( _createRepo( alias_r ) );
    return res;
  }

  bool Pool::reposEmpty() const
  { return _pool->urepos == 0; }

  detail::size_type Pool::reposSize() const
  { return _pool->urepos; }

  Pool::RepositoryIterable Pool::repos() const
  {
    detail::RepositoryIterator start, end  = detail::RepositoryIterator( _pool->repos + _pool->nrepos );
    if ( _pool->urepos )
    { // repos[0] == NULL
      for( auto it = _pool->repos+1; it != _pool->repos+_pool->nrepos; it++ )
        if ( *it ) {
          start = detail::RepositoryIterator( it );
          break;
        }
    }
    return RepositoryIterable( start, end );
  }

  Repository Pool::findSystemRepo() const
  { return Repository( _pool->installed ); }

  Repository Pool::systemRepo()
  {
    if ( ! _pool->installed )
      _createRepo( systemRepoAlias() );
    return Repository( _pool->installed );
  }

  bool Pool::solvablesEmpty() const
  {
    // return myPool()->nsolvables;
    // nsolvables is the array size including
    // invalid Solvables.
    for( const auto &repo : repos() )
    {
      if ( ! repo.solvablesEmpty() )
        return false;
    }
    return true;
  }

  detail::size_type Pool::solvablesSize() const
  {
    // Do not return myPool()->nsolvables;
    // nsolvables is the array size including
    // invalid Solvables.
    detail::size_type ret = 0;
    for( const auto &repo : repos() )
    {
      ret += repo.solvablesSize();
    }
    return ret;
  }

  Pool::SolvableIterable Pool::solvables() const
  {
    return Pool::SolvableIterable(
          detail::SolvableIterator( getFirstId() ),
          detail::SolvableIterator()
    );
  }

  Queue Pool::whatMatchesDep( const SolvAttr &attr, const Capability &cap ) const
  {
    sat::Queue q;
    pool_whatmatchesdep( _pool, attr.id(), cap.id(), q, 0);
    return q;
  }

  Queue Pool::whatMatchesSolvable ( const SolvAttr &attr, const Solvable &solv ) const
  {
    sat::Queue q;
    pool_whatmatchessolvable( _pool, attr.id(), static_cast<Id>( solv.id() ), q, 0 );
    return q;
  }

  Queue Pool::whatContainsDep ( const SolvAttr &attr, const Capability &cap ) const
  {
    sat::Queue q;
    pool_whatcontainsdep( _pool, attr.id(), cap.id(), q, 0 );
    return q;
  }

  detail::CRepo *Pool::_createRepo(const std::string &name_r)
  {
    setDirty( PoolInvalidation::Data, { __FUNCTION__, name_r } );
    detail::CRepo * ret = ::repo_create( _pool, name_r.c_str() );
    if ( ret && name_r == systemRepoAlias() )
      ::pool_set_installed( _pool, ret );
    return ret;
  }

  void Pool::_deleteRepo(detail::CRepo *repo_r)
  {
    setDirty( PoolInvalidation::Data, { __FUNCTION__, repo_r->name } );

    _componentsSet.notifyRepoRemoved( *this, repo_r );

    ::repo_free( repo_r, /*resusePoolIDs*/false );
    if ( !_pool->urepos )
    {
      _serialIDs.setDirty();
      ::pool_freeallrepos( _pool, /*resusePoolIDs*/true );
    }
  }

  int Pool::_addSolv(detail::CRepo *repo_r, FILE *file_r)
  {
    setDirty( PoolInvalidation::Data, { __FUNCTION__, repo_r->name } );
    int ret = ::repo_add_solv( repo_r, file_r, 0 );
    if ( ret == 0 )
      _postRepoAdd( repo_r );
    return ret;
  }

  int Pool::_addHelix(detail::CRepo *repo_r, FILE *file_r)
  {
    setDirty( PoolInvalidation::Data, { __FUNCTION__, repo_r->name } );
    int ret = ::repo_add_helix( repo_r, file_r, 0 );
    if ( ret == 0 )
      _postRepoAdd( repo_r );
    return ret;
  }

  int Pool::_addTesttags(detail::CRepo *repo_r, FILE *file_r)
  {
    setDirty( PoolInvalidation::Data, { __FUNCTION__, repo_r->name } );
    int ret = ::testcase_add_testtags( repo_r, file_r, 0 );
    if ( ret == 0 )
      _postRepoAdd( repo_r );
    return ret;
  }

  detail::SolvableIdType Pool::_addSolvables(detail::CRepo *repo_r, unsigned int count_r)
  {
    setDirty( PoolInvalidation::Data, { __FUNCTION__, repo_r->name } );
    return ::repo_add_solvable_block( repo_r, count_r );
  }

  void Pool::_postRepoAdd( detail::CRepo * repo_r )
  {
    _componentsSet.notifyRepoAdded( *this, repo_r );
  }

} // namespace zyppng::sat
