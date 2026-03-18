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
#include "preparedpool.h"

#include <zypp-core/base/LogTools.h>
#include <zypp-core/base/dtorreset.h>
#include <zypp-core/ng/base/precondition.h>
#include <zypp/ng/sat/capability.h>

extern "C"
{
#include <solv/repo_helix.h>
#include <solv/testcase.h>
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
      ZYPP_PRECONDITION( _pool->appdata == nullptr, "Only one zyppng::sat::Pool instance per CPool is permitted" );
      _pool->appdata = this;
      // TODO add logging functions
  }

  Pool::~Pool()
  {
      clear();
      _pool->appdata = nullptr;
  }

  detail::size_type Pool::capacity() const
  { return _pool->nsolvables; }

  PreparedPool Pool::prepare()
  {
    // Pass 1: let components probe external state.
    // setDirty() is legal here — onInvalidate() fires synchronously,
    // so all components see a consistent state before prepare starts.
    _componentsSet.notifyCheckDirty( *this );

#ifndef NDEBUG
    zypp::DtorReset preparingReset( _preparing, false );
    _preparing = true;
#endif

    // Pass 2: pre-index component work — only if data changed.
    const bool serialDirty = _watcher.remember( _serial );
    if ( serialDirty )
      _componentsSet.notifyPrepare( *this );

    // Pass 3: rebuild index if cleared (independent of serial — can be
    // invalidated by onInvalidate(Dependency) without a data change).
    const bool indexRebuilt = ( _pool->whatprovides == nullptr );
    if ( indexRebuilt ) {
      MIL << "pool_createwhatprovides..." << std::endl;
      ::pool_addfileprovides( _pool );
      ::pool_createwhatprovides( _pool );
    }

    // Early exit — nothing changed and index is valid.
    if ( !serialDirty && !indexRebuilt )
      return PreparedPool( *this );

    // Pass 4: post-index component work — serial changed or index was rebuilt.
    PreparedPool pp( *this );
    _componentsSet.notifyPrepareWithIndex( pp );

    return pp;
  }

  void Pool::clear()
  {
    _componentsSet.notifyReset( *this );
    ::pool_freewhatprovides( _pool );
    ::pool_freeallrepos( _pool, /*resusePoolIDs*/true );
    _serialIDs.setDirty();
    _serial.setDirty();
  }

  void Pool::setDirty( PoolInvalidation invalidation, std::initializer_list<std::string_view> reasons )
  {
#ifndef NDEBUG
      ZYPP_PRECONDITION( !_preparing, "setDirty() called during prepare() — only legal in checkDirty()" );
#endif
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
    // Do not return get()->nsolvables;
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
    // Do not return get()->nsolvables;
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

  detail::CRepo *Pool::_createRepo(const std::string &name_r)
  {
    setDirty( PoolInvalidation::Data, { __FUNCTION__, name_r } );
    detail::CRepo * ret = ::repo_create( _pool, name_r.c_str() );
    if ( ret && name_r == systemRepoAlias() )
      ::pool_set_installed( _pool, ret );
    if ( ret )
      _postRepoAdd( ret );
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
