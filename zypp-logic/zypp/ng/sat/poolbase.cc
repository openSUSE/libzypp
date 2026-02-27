/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "poolbase.h"
#include "stringpool.h"
#include "components/namespacecomponent.h"
#include "components/architecturecomponent.h"
#include "components/autoinstalledcomponent.h"
#include "components/packagepolicycomponent.h"


#include <boost/mpl/int.hpp>
#include <boost/mpl/assert.hpp>

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
    // MPL checks for satlib constants we redefine to avoid
    // includes and defines.
    BOOST_MPL_ASSERT_RELATION( noId,                 ==, STRID_NULL );
    BOOST_MPL_ASSERT_RELATION( emptyId,              ==, STRID_EMPTY );

    BOOST_MPL_ASSERT_RELATION( noSolvableId,         ==, ID_NULL );
    BOOST_MPL_ASSERT_RELATION( systemSolvableId,     ==, SYSTEMSOLVABLE );

    BOOST_MPL_ASSERT_RELATION( solvablePrereqMarker, ==, SOLVABLE_PREREQMARKER );
    BOOST_MPL_ASSERT_RELATION( solvableFileMarker,   ==, SOLVABLE_FILEMARKER );

    BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_AND,       ==, REL_AND );
    BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_OR,        ==, REL_OR );
    BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_COND,      ==, REL_COND );
    BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_UNLESS,    ==, REL_UNLESS );
    BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_ELSE,      ==, REL_ELSE );
    BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_WITH,      ==, REL_WITH );
    BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_WITHOUT,   ==, REL_WITHOUT );
    BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_NAMESPACE, ==, REL_NAMESPACE );
    BOOST_MPL_ASSERT_RELATION( CapDetail::CAP_ARCH,      ==, REL_ARCH );

    BOOST_MPL_ASSERT_RELATION( namespaceModalias,	==, NAMESPACE_MODALIAS );
    BOOST_MPL_ASSERT_RELATION( namespaceLanguage,	==, NAMESPACE_LANGUAGE );
    BOOST_MPL_ASSERT_RELATION( namespaceFilesystem,	==, NAMESPACE_FILESYSTEM );


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

  PoolBase::PoolBase()
    : _pool( StringPool::instance().getPool() )
  {
    _componentsSet.assertComponent<NamespaceComponent>();
    _componentsSet.assertComponent<ArchitectureComponent>();
    _componentsSet.assertComponent<AutoInstalledComponent>();
    _componentsSet.assertComponent<PackagePolicyComponent>();
  }

  PoolBase::~PoolBase()
  {

  }

  void PoolBase::prepare()
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

  void PoolBase::setDirty( PoolInvalidation invalidation, std::initializer_list<std::string_view> reasons )
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

  const std::string &PoolBase::systemRepoAlias()
  {
    static const std::string _val( "@System" );
    return _val;
  }

  detail::CRepo *PoolBase::_createRepo(const std::string &name_r)
  {
    setDirty( PoolInvalidation::Data, { __FUNCTION__, name_r } );
    detail::CRepo * ret = ::repo_create( _pool, name_r.c_str() );
    if ( ret && name_r == systemRepoAlias() )
      ::pool_set_installed( _pool, ret );
    return ret;
  }

  void PoolBase::_deleteRepo(detail::CRepo *repo_r)
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

  int PoolBase::_addSolv(detail::CRepo *repo_r, FILE *file_r)
  {
    setDirty( PoolInvalidation::Data, { __FUNCTION__, repo_r->name } );
    int ret = ::repo_add_solv( repo_r, file_r, 0 );
    if ( ret == 0 )
      _postRepoAdd( repo_r );
    return ret;
  }

  int PoolBase::_addHelix(detail::CRepo *repo_r, FILE *file_r)
  {
    setDirty( PoolInvalidation::Data, { __FUNCTION__, repo_r->name } );
    int ret = ::repo_add_helix( repo_r, file_r, 0 );
    if ( ret == 0 )
      _postRepoAdd( repo_r );
    return ret;
  }

  int PoolBase::_addTesttags(detail::CRepo *repo_r, FILE *file_r)
  {
    setDirty( PoolInvalidation::Data, { __FUNCTION__, repo_r->name } );
    int ret = ::testcase_add_testtags( repo_r, file_r, 0 );
    if ( ret == 0 )
      _postRepoAdd( repo_r );
    return ret;
  }

  detail::SolvableIdType PoolBase::_addSolvables(detail::CRepo *repo_r, unsigned int count_r)
  {
    setDirty( PoolInvalidation::Data, { __FUNCTION__, repo_r->name } );
    return ::repo_add_solvable_block( repo_r, count_r );
  }

  void PoolBase::_postRepoAdd( detail::CRepo * repo_r )
  {
    _componentsSet.notifyRepoAdded( *this, repo_r );
  }

} // namespace zyppng::sat
