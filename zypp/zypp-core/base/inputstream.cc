/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/base/InputStream.cc
 *
*/
#include <iostream>
#include <zypp-core/base/LogTools.h>

#include "inputstream.h"
#include <utility>
#include <zypp-core/base/GzStream>

#ifdef ENABLE_ZCHUNK_COMPRESSION
  #include <zypp-core/base/ZckStream>
#endif

#include <zypp-core/fs/PathInfo.h>

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  namespace
  { /////////////////////////////////////////////////////////////////

    inline std::streamoff _helperInitSize( const Pathname & file_r )
    {
      PathInfo p( file_r );
      if ( p.isFile() && filesystem::zipType( file_r ) == filesystem::ZT_NONE )
        return p.size();
      return -1;
    }

    inline shared_ptr<std::istream> streamForFile ( const Pathname & file_r )
    {
#ifdef ENABLE_ZCHUNK_COMPRESSION
      if ( const auto zType = filesystem::zipType( file_r ); zType == filesystem::ZT_ZCHNK )
        return shared_ptr<std::istream>( new ifzckstream( file_r.asString().c_str() ) );
#endif

      //fall back to gzstream
      return shared_ptr<std::istream>( new ifgzstream( file_r.asString().c_str() ) );
    }

    /////////////////////////////////////////////////////////////////
  } // namespace
  ///////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : InputStream::InputStream
  //	METHOD TYPE : Constructor
  //
  InputStream::InputStream()
  : _stream( &std::cin, NullDeleter() )
  , _name( "STDIN" )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : InputStream::InputStream
  //	METHOD TYPE : Constructor
  //
  InputStream::InputStream( std::istream & stream_r,
                            std::string  name_r )
  : _stream( &stream_r, NullDeleter() )
  , _name(std::move( name_r ))
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : InputStream::InputStream
  //	METHOD TYPE : Constructor
  //
  InputStream::InputStream( Pathname  file_r )
  : _path(std::move( file_r ))
  , _stream( streamForFile( _path.asString() ) )
  , _name( _path.asString() )
  , _size( _helperInitSize( _path ) )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : InputStream::InputStream
  //	METHOD TYPE : Constructor
  //
  InputStream::InputStream( Pathname  file_r,
                            std::string  name_r )
  : _path(std::move( file_r ))
  , _stream( streamForFile( _path.asString() ) )
  , _name(std::move( name_r ))
  , _size( _helperInitSize( _path ) )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : InputStream::InputStream
  //	METHOD TYPE : Constructor
  //
  InputStream::InputStream( const std::string & file_r )
  : _path( file_r )
  , _stream( streamForFile( _path.asString() ) )
  , _name( _path.asString() )
  , _size( _helperInitSize( _path ) )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : InputStream::InputStream
  //	METHOD TYPE : Constructor
  //
  InputStream::InputStream( const std::string & file_r,
                            std::string  name_r )
  : _path( file_r )
  , _stream( streamForFile( _path.asString() ) )
  , _name(std::move( name_r ))
  , _size( _helperInitSize( _path ) )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : InputStream::InputStream
  //	METHOD TYPE : Constructor
  //
  InputStream::InputStream( const char * file_r )
  : _path( file_r )
  , _stream( streamForFile( _path.asString() ) )
  , _name( _path.asString() )
  , _size( _helperInitSize( _path ) )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : InputStream::InputStream
  //	METHOD TYPE : Constructor
  //
  InputStream::InputStream( const char * file_r,
                            std::string  name_r )
  : _path( file_r )
  , _stream( streamForFile( _path.asString() ) )
  , _name(std::move( name_r ))
  , _size( _helperInitSize( _path ) )
  {}

  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : InputStream::~InputStream
  //	METHOD TYPE : Destructor
  //
  InputStream::~InputStream()
  {}

  /******************************************************************
   **
   **	FUNCTION NAME : operator<<
   **	FUNCTION TYPE : std::ostream &
  */
  std::ostream & operator<<( std::ostream & str, const InputStream & obj )
  {
    return str << obj.name() << obj.stream();
  }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
