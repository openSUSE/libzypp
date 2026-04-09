/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "architecturecomponent.h"
#include <zypp/ng/sat/pool.h>
#include <set>

extern "C"
{
#include <solv/poolarch.h>
#include <solv/repo.h>
}

namespace zyppng::sat {

  void ArchitectureComponent::prepare( Pool & pool )
  {
    if ( _watcher.remember( pool.serial() ) )  {
      ::pool_setarch( pool.get(), arch().asString().c_str() );
    }
  }

  void ArchitectureComponent::onRepoAdded( Pool & pool, detail::RepoIdType id )
  {
    detail::CRepo * repo_r = id;
    if ( ! pool.isSystemRepo( repo_r ) )
    {
      // Filter out unwanted archs
      std::set<detail::IdType> sysids;
      {
        zypp::Arch arch = this->arch();
        zypp::Arch::CompatSet sysarchs( zypp::Arch::compatSet( arch ) );
        for ( auto it = sysarchs.begin(); it != sysarchs.end(); ++it )
          sysids.insert( it->id() );

        // unfortunately libsolv treats src/nosrc as architecture:
        sysids.insert( ARCH_SRC );
        sysids.insert( ARCH_NOSRC );
      }

      detail::IdType blockBegin = 0;
      unsigned       blockSize  = 0;
      for ( detail::IdType i = repo_r->start; i < repo_r->end; ++i )
      {
        detail::CSolvable * s( pool.get()->solvables + i );
        if ( s->repo == repo_r && sysids.find( s->arch ) == sysids.end() )
        {
          // Remember an unwanted arch entry:
          if ( ! blockBegin )
            blockBegin = i;
          ++blockSize;
        }
        else if ( blockSize )
        {
          // Free remembered entries
          ::repo_free_solvable_block( repo_r, blockBegin, blockSize, /*resusePoolIDs*/false );
          blockBegin = blockSize = 0;
        }
      }
      if ( blockSize ) {
        // Free remembered entries
        ::repo_free_solvable_block( repo_r, blockBegin, blockSize, /*resusePoolIDs*/false );
      }
    }
  }

} // namespace zyppng::sat
