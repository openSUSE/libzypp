/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/target/CommitPackageCache.cc
 *
*/
#include <iostream>
#include "zypp/base/Logger.h"
#include "zypp/base/Exception.h"

#include "zypp/target/CommitPackageCache.h"

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace target
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : CommitPackageCache::Impl
    //
    /** Base for CommitPackageCache implementations. */
    class CommitPackageCache::Impl
    {
    public:
      typedef CommitPackageCache::const_iterator   const_iterator;
      typedef CommitPackageCache::PackageProvider  PackageProvider;

    public:
      Impl( const PackageProvider & packageProvider_r )
      : _packageProvider( packageProvider_r )
      {}

      virtual ~Impl()
      {}

    public:
      virtual ManagedFile get( const_iterator citem_r ) = 0;

    protected:
      /** Let the Source provide the package. */
      ManagedFile sourceProvidePackage( const PoolItem & pi )
      {
        if ( ! _packageProvider )
          {
            ZYPP_THROW( Exception("No package provider configured.") );
          }

        ManagedFile ret( _packageProvider( pi ) );
        if ( ret.value().empty() )
          {
            ZYPP_THROW( Exception("Package provider failed.") );
          }

        return ret;
      }

    private:
      PackageProvider _packageProvider;
    };
    ///////////////////////////////////////////////////////////////////

    /** \relates CommitPackageCache::Impl Stream output */
    inline std::ostream & operator<<( std::ostream & str, const CommitPackageCache::Impl & obj )
    {
      return str << "CommitPackageCache::Impl";
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : NoCommitPackageCache
    //
    ///////////////////////////////////////////////////////////////////
    /** CommitPackageCache caching nothing at all.
     * This is a NOOP. All packages are directly retrieved from the
     * source.
    */
    struct NoCommitPackageCache : public CommitPackageCache::Impl
    {
      NoCommitPackageCache( const PackageProvider & packageProvider_r )
      : CommitPackageCache::Impl( packageProvider_r )
      {}

      virtual ManagedFile get( const_iterator citem_r )
      { return sourceProvidePackage( *citem_r ); }
    };

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : CommitPackageCache
    //
    ///////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : CommitPackageCache::CommitPackageCache
    //	METHOD TYPE : Ctor
    //
    CommitPackageCache::CommitPackageCache( const_iterator          begin_r,
                                            const_iterator          end_r,
                                            const Pathname &        rootDir_r,
                                            const PackageProvider & packageProvider_r )
    : _pimpl( new NoCommitPackageCache( packageProvider_r ) )
    {}

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : CommitPackageCache::~CommitPackageCache
    //	METHOD TYPE : Dtor
    //
    CommitPackageCache::~CommitPackageCache()
    {}

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : CommitPackageCache::~CommitPackageCache
    //	METHOD TYPE : Dtor
    //
    ManagedFile CommitPackageCache::get( const_iterator citem_r )
    { return _pimpl->get( citem_r ); }

    /******************************************************************
    **
    **	FUNCTION NAME : operator<<
    **	FUNCTION TYPE : std::ostream &
    */
    std::ostream & operator<<( std::ostream & str, const CommitPackageCache & obj )
    { return str << *obj._pimpl; }

    /////////////////////////////////////////////////////////////////
  } // namespace target
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
