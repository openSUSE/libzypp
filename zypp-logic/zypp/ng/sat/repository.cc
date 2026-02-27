/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ng/sat/repository.cc
 *
*/
#include <climits>
#include <iostream>
#include <utility>

#include <zypp-core/base/LogTools.h>
#include <zypp-core/base/Gettext.h>
#include <zypp-core/base/Exception.h>

#include <zypp-core/AutoDispose.h>
#include <zypp-core/Pathname.h>

#include <zypp/ng/sat/poolbase.h>
#include <zypp/ng/sat/repository.h>
#include <zypp/ng/sat/lookupattr.h>

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zyppng
{ /////////////////////////////////////////////////////////////////
  namespace sat
  {

    const Repository Repository::noRepository;

    const std::string & Repository::systemRepoAlias()
    { return PoolBase::systemRepoAlias(); }

    /////////////////////////////////////////////////////////////////

    detail::CRepo * Repository::get() const
    { return _id; }

#define NO_REPOSITORY_RETURN( VAL ) \
    detail::CRepo * _repo( get() ); \
    if ( ! _repo ) return VAL

#define NO_REPOSITORY_THROW( VAL ) \
    detail::CRepo * _repo( get() ); \
    if ( ! _repo ) ZYPP_THROW( VAL )

    bool Repository::isSystemRepo() const
    {
        NO_REPOSITORY_RETURN( false );
        return myPool().isSystemRepo( _repo );
    }

    std::string Repository::alias() const
    {
      NO_REPOSITORY_RETURN( std::string() );
      if ( ! _repo->name )
        return std::string();
      return _repo->name;
    }

    int Repository::satInternalPriority() const
    {
      NO_REPOSITORY_RETURN( INT_MIN );
      return _repo->priority;
    }

    int Repository::satInternalSubPriority() const
    {
      NO_REPOSITORY_RETURN( INT_MIN );
      return _repo->subpriority;
    }

    Repository::ContentRevision Repository::contentRevision() const
    {
      NO_REPOSITORY_RETURN( ContentRevision() );
      sat::LookupRepoAttr q( sat::SolvAttr::repositoryRevision, *this );
      return q.empty() ? std::string() : q.begin().asString();
    }

    Repository::ContentIdentifier Repository::contentIdentifier() const
    {
      NO_REPOSITORY_RETURN( ContentIdentifier() );
      sat::LookupRepoAttr q( sat::SolvAttr::repositoryRepoid, *this );
      return q.empty() ? std::string() : q.begin().asString();
    }

    bool Repository::hasContentIdentifier( const ContentIdentifier & id_r ) const
    {
      NO_REPOSITORY_RETURN( false );

      sat::LookupRepoAttr q( sat::SolvAttr::repositoryRepoid, *this );
      for_( it, q.begin(), q.end() )
        if ( it.asString() == id_r )
          return true;

      return false;
    }

    zypp::Date Repository::generatedTimestamp() const
    {
      NO_REPOSITORY_RETURN( zypp::Date() );
      sat::LookupRepoAttr q( sat::SolvAttr::repositoryTimestamp, *this );
      return( q.empty() ? 0 : q.begin().asUnsigned() );

    }

    zypp::Date Repository::suggestedExpirationTimestamp() const
    {
      NO_REPOSITORY_RETURN( zypp::Date() );
      zypp::Date generated = generatedTimestamp();
      if ( ! generated )
        return 0; // do not calculate over a missing generated timestamp

      sat::LookupRepoAttr q( sat::SolvAttr::repositoryExpire, *this );
      if ( q.empty() )
        return 0;

      return generated + zypp::Date(q.begin().asUnsigned());

    }

    /*
    Repository::Keywords Repository::keywords() const
    {
      NO_REPOSITORY_RETURN( Keywords() );
      return Keywords( sat::SolvAttr::repositoryKeywords, *this, sat::LookupAttr::REPO_ATTR );
    }

    bool Repository::hasKeyword( const std::string & val_r ) const
    {
      for ( const auto & val : keywords() )
        if ( val == val_r )
          return true;
      return false;
    }
    */

    bool Repository::maybeOutdated() const
    {
      NO_REPOSITORY_RETURN( false );
      // system repo is not mirrored
      if ( isSystemRepo() )
        return false;

      zypp::Date suggested = suggestedExpirationTimestamp();

      // if no data, don't suggest
      if ( ! suggested )
        return false;

      return suggestedExpirationTimestamp() < zypp::Date::now();
    }

    /*
    bool Repository::providesUpdatesFor( const CpeId & cpeid_r ) const
    {
      NO_REPOSITORY_RETURN( false );
      if ( ! cpeid_r )
        return false;	// filter queries/products without CpeId, as an empty CpeId matches ANYthing.

      // check in repository metadata
      for_( it, updatesProductBegin(), updatesProductEnd() )
      {
        if ( compare( cpeid_r, it.cpeId(), SetRelation::subset ) )
          return true;
      }

      // check whether known products refer to this as update repo
      sat::LookupRepoAttr myIds( sat::SolvAttr::repositoryRepoid, *this );	// usually just one, but...
      if ( ! myIds.empty() )
      {
        const ResPool & pool( ResPool::instance() );
        for_( it, pool.byKindBegin<Product>(), pool.byKindEnd<Product>() )
        {
          Product::constPtr prod( (*it)->asKind<Product>() );
          if ( compare( cpeid_r, prod->cpeId(), SetRelation::superset ) )
          {
            for_( myId, myIds.begin(), myIds.end() )
            {
              if ( prod->hasUpdateContentIdentifier( myId.asString() ) )
                return true;
            }
          }
        }
      }
      return false;
    }

    bool Repository::isUpdateRepo() const
    {
      NO_REPOSITORY_RETURN( false );

      // check in repository metadata
      if ( updatesProductBegin() != updatesProductEnd() )
        return true;

      // check whether known products refer to this as update repo
      sat::LookupRepoAttr myIds( sat::SolvAttr::repositoryRepoid, *this );	// usually just one, but...
      if ( ! myIds.empty() )
      {
        const ResPool & pool( ResPool::instance() );
        for_( it, pool.byKindBegin<Product>(), pool.byKindEnd<Product>() )
        {
          for_( myId, myIds.begin(), myIds.end() )
          {
            if ( (*it)->asKind<Product>()->hasUpdateContentIdentifier( myId.asString() ) )
              return true;
          }
        }
      }
      return false;
    }
    */

    bool Repository::solvablesEmpty() const
    {
      NO_REPOSITORY_RETURN( true );
      return !_repo->nsolvables;
    }

    Repository::size_type Repository::solvablesSize() const
    {
      NO_REPOSITORY_RETURN( 0 );
      return _repo->nsolvables;
    }

    Repository::SolvableIterator Repository::solvablesBegin() const
    {
      NO_REPOSITORY_RETURN( zypp::make_filter_iterator( detail::ByRepository( *this ),
                            sat::detail::SolvableIterator(),
                            sat::detail::SolvableIterator() ) );
      return zypp::make_filter_iterator( detail::ByRepository( *this ),
                                   sat::detail::SolvableIterator(_repo->start),
                                   sat::detail::SolvableIterator(_repo->end) );
    }

    Repository::SolvableIterator Repository::solvablesEnd() const
    {
      NO_REPOSITORY_RETURN( zypp::make_filter_iterator( detail::ByRepository( *this ),
                            sat::detail::SolvableIterator(),
                            sat::detail::SolvableIterator() ) );
      return zypp::make_filter_iterator(detail::ByRepository( *this ),
                                  sat::detail::SolvableIterator(_repo->end),
                                  sat::detail::SolvableIterator(_repo->end) );
    }

    /*
    Repository::ProductInfoIterator Repository::compatibleWithProductBegin() const
    {
      NO_REPOSITORY_RETURN( ProductInfoIterator() );
      return ProductInfoIterator( sat::SolvAttr::repositoryDistros, *this );
    }

    Repository::ProductInfoIterator Repository::compatibleWithProductEnd() const
    {
      return ProductInfoIterator();
    }

    Repository::ProductInfoIterator Repository::updatesProductBegin() const
    {
      NO_REPOSITORY_RETURN( ProductInfoIterator() );
      return ProductInfoIterator( sat::SolvAttr::repositoryUpdates, *this );
    }

    Repository::ProductInfoIterator Repository::updatesProductEnd() const
    {
      return ProductInfoIterator();
    }
    */

    /*
    RepoInfo Repository::info() const
    {
      NO_REPOSITORY_RETURN( RepoInfo() );
      return myPool().repoInfo( _repo );
    }

    void Repository::setInfo( const RepoInfo & info_r )
    {
        NO_REPOSITORY_THROW( Exception( "Can't set RepoInfo for norepo." ) );
        if ( info_r.alias() != alias() )
        {
            ZYPP_THROW( Exception( str::form( "RepoInfo alias (%s) does not match repository alias (%s)",
                                              info_r.alias().c_str(), alias().c_str() ) ) );
        }
        myPool().setRepoInfo( _repo, info_r );
        MIL << *this << endl;
    }

    void Repository::clearInfo()
    {
        NO_REPOSITORY_RETURN();
        myPool().setRepoInfo( _repo, RepoInfo() );
    }
    */

    void Repository::eraseFromPool()
    {
        NO_REPOSITORY_RETURN();
        //MIL << *this << " removed from pool" << endl;
        myPool()._deleteRepo( _repo );
        _id = detail::noRepoId;
    }

    Repository Repository::nextInPool() const
    {
      NO_REPOSITORY_RETURN( noRepository );
      /*
      for_( it, sat::Pool::instance().reposBegin(), sat::Pool::instance().reposEnd() )
      {
        if ( *it == *this )
        {
          if ( ++it != _for_end )
            return *it;
          break;
        }
      }
      */
      return noRepository;
    }

    void Repository::addSolv( const zypp::Pathname & file_r )
    {
      NO_REPOSITORY_THROW( zypp::Exception( "Can't add solvables to norepo." ) );

      zypp::AutoDispose<FILE*> file( ::fopen( file_r.c_str(), "re" ), ::fclose );
      if ( file == NULL )
      {
        file.resetDispose();
        ZYPP_THROW( zypp::Exception( "Can't open solv-file: "+file_r.asString() ) );
      }

      if ( myPool()._addSolv( _repo, file ) != 0 )
      {
        ZYPP_THROW( zypp::Exception( "Error reading solv-file: "+file_r.asString() ) );
      }

      //MIL << *this << " after adding " << file_r << endl;
    }

    void Repository::addHelix( const zypp::Pathname & file_r )
    {
      NO_REPOSITORY_THROW( zypp::Exception( "Can't add solvables to norepo." ) );

      std::string command( file_r.extension() == ".gz" ? "zcat " : "cat " );
      command += file_r.asString();

      zypp::AutoDispose<FILE*> file( ::popen( command.c_str(), "re" ), ::pclose );
      if ( file == NULL )
      {
        file.resetDispose();
        ZYPP_THROW( zypp::Exception( "Can't open helix-file: "+file_r.asString() ) );
      }

      if ( myPool()._addHelix( _repo, file ) != 0 )
      {
        ZYPP_THROW( zypp::Exception( "Error reading helix-file: "+file_r.asString() ) );
      }

      //MIL << *this << " after adding " << file_r << endl;
    }

    void Repository::addTesttags( const zypp::Pathname & file_r )
    {
      NO_REPOSITORY_THROW( zypp::Exception( "Can't add solvables to norepo." ) );

      std::string command( file_r.extension() == ".gz" ? "zcat " : "cat " );
      command += "'";
      command += file_r.asString();
      command += "'";

      zypp::AutoDispose<FILE*> file( ::popen( command.c_str(), "re" ), ::pclose );
      if ( file == NULL )
      {
        file.resetDispose();
        ZYPP_THROW( zypp::Exception( "Can't open testtags-file: "+file_r.asString() ) );
      }

      if ( myPool()._addTesttags( _repo, file ) != 0 )
      {
        ZYPP_THROW( zypp::Exception( "Error reading testtags-file: "+file_r.asString() ) );
      }

      //MIL << *this << " after adding " << file_r << endl;
    }

    sat::Solvable::IdType Repository::addSolvables( unsigned count_r )
    {
        NO_REPOSITORY_THROW( zypp::Exception( "Can't add solvables to norepo.") );
        return myPool()._addSolvables( _repo, count_r );
    }

  } // namespace sat
} // namespace zyppng
