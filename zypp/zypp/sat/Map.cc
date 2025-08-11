/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/Map.cc
 */
extern "C"
{
#include <solv/bitmap.h>
}
#include <iostream>
#include <exception>
#include <zypp/base/LogTools.h>
#include <zypp/base/String.h>

#include <zypp/sat/Map.h>
#include <zypp/sat/Pool.h>

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  template<>
  sat::detail::CMap * rwcowClone<sat::detail::CMap>( const sat::detail::CMap * rhs )
  {
    sat::detail::CMap * ret = new sat::detail::CMap;
    ::map_init_clone( ret, const_cast<sat::detail::CMap *>(rhs) );
    return ret;
  }

  ///////////////////////////////////////////////////////////////////
  namespace sat
  { /////////////////////////////////////////////////////////////////

  constexpr Map::PoolSizeType Map::poolSize;

    Map::Map()
      : _pimpl( new detail::CMap )
    { ::map_init( _pimpl.get(), 0 ); }

    Map::Map( size_type size_r )
      : _pimpl( new detail::CMap )
    { ::map_init( _pimpl.get(), size_r ); }

    Map::Map( PoolSizeType )
      : _pimpl( new detail::CMap )
    { ::map_init( _pimpl.get(), Pool::instance().capacity() ); }

    Map::~Map()
    { ::map_free( _pimpl.get() ); }

    bool Map::empty() const
    { return( _pimpl->size == 0 ); }

    Map::size_type Map::size() const
    { return _pimpl->size << 3; }

    void Map::grow( size_type size_r )
    { ::map_grow( _pimpl.get(), size_r ); }

    void Map::setAll()
    { assignAll( true ); }

    void Map::clearAll()
    { assignAll( false ); }

    void Map::assignAll( bool val_r )
    {
      if ( _pimpl->size )
        ::memset( _pimpl->map, (val_r?-1:0), _pimpl->size );
    }

#define M_RANGE_CKECK(IDX,LOC) if ( ((IDX) >> 3) >= size_type(_pimpl->size) ) throw std::out_of_range( "zypp::sat::Map::" LOC )

    void Map::set( size_type idx_r )
    {
      M_RANGE_CKECK( idx_r, "set" );
      MAPSET( _pimpl, idx_r );
    }

    void Map::clear( size_type idx_r )
    {
      M_RANGE_CKECK( idx_r, "clear" );
      MAPCLR( _pimpl, idx_r );
    }

    void Map::assign( size_type idx_r, bool val_r )
    {
      M_RANGE_CKECK( idx_r, "assign" );
      if ( val_r )
      { MAPSET( _pimpl, idx_r ); }
      else
      { MAPCLR( _pimpl, idx_r ); }
    }

    bool Map::test( size_type idx_r ) const
    {
      M_RANGE_CKECK( idx_r, "test" );
      return MAPTST( _pimpl, idx_r );
    }

    std::string Map::asString( const char on_r, const char off_r ) const
    {
      if ( empty() )
        return std::string();

      std::string ret( size(), off_r );
      for_( idx, size_type(0), size() )
      {
        if ( test( idx ) )
          ret[idx] = on_r;
      }
      return ret;
    }

    Map::operator detail::CMap *()	// COW: nonconst version can't be inlined
    { return _pimpl.get(); }		// without exposing detail::CMap

    bool operator==( const Map & lhs, const Map & rhs )
    {
      const detail::CMap * l = lhs;
      const detail::CMap * r = rhs;
      return( l == r || ( l->size == r->size && ::memcmp( l->map, r->map, l->size ) == 0 ) );
    }

    /////////////////////////////////////////////////////////////////
  } // namespace sat
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
