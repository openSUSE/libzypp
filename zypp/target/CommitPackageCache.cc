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
#include "zypp/target/CommitPackageCacheImpl.h"

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace target
  { /////////////////////////////////////////////////////////////////

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
    CommitPackageCache::CommitPackageCache( Impl * pimpl_r )
    : _pimpl( pimpl_r )
    { assert( pimpl_r ); }

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : CommitPackageCache::CommitPackageCache
    //	METHOD TYPE : Ctor
    //
    CommitPackageCache::CommitPackageCache( const_iterator          begin_r,
                                            const_iterator          end_r,
                                            const Pathname &        rootDir_r,
                                            const PackageProvider & packageProvider_r )
    : _pimpl( new Impl( packageProvider_r ) )
    {
      // here install the "real" cache.
    }

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
