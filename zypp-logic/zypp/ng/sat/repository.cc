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

#include <zypp/ng/sat/pool.h>
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
    { return Pool::systemRepoAlias(); }

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

      return suggested < zypp::Date::now();
    }

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
      auto &pool = myPool();
      auto iterable = pool.repos();
      for( auto it = iterable.begin(); it != iterable.end(); ++it )
      {
        if ( *it == *this )
        {
          if ( ++it != iterable.end() )
            return *it;
          break;
        }
      }
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

    namespace detail
    {
      void RepositoryIterator::increment()
      {
        if ( base() )
        {
          sat::detail::CPool * satpool = (*base())->pool;
          do {
            ++base_reference();
          } while ( base() < satpool->repos+satpool->nrepos && !*base() );
        }
      }
    }

  } // namespace sat
} // namespace zyppng
