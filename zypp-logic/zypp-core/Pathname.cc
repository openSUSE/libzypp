/*---------------------------------------------------------------------\
 |                          ____ _   __ __ ___                          |
 |                         |__  / \ / / . \ . \                         |
 |                           / / \ V /|  _/  _/                         |
 |                          / /__ | | | | | |                           |
 |                         /_____||_| |_| |_|                           |
 |                                                                      |
 \---------------------------------------------------------------------*/
/** \file	zypp/Pathname.cc
 *
*/
#include <iostream>
#include <climits>

#include <zypp-core/base/String.h>
#include <zypp-core/Pathname.h>
#include <zypp-core/Url.h>

using std::string;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace filesystem
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : Pathname::_assign
    //	METHOD TYPE : void
    //
    void Pathname::_assign( const string & name_r )
    {
      _name.clear();
      if ( name_r.empty() )
        return;
      _name.reserve( name_r.size() );

      // Collect up to "/.."
      enum Pending {
        P_none	= 0,	// ""
        P_slash	= 1,	// "/"
        P_dot1	= 2,	// "/."
        P_dot2	= 3	// "/.."
      } pending = P_none;

      // Assert relative path starting with "./"
      // We rely on this below!
      if ( name_r[0] != '/' )
      {
        _name += '.';
        pending = P_slash;
      }

      // Lambda handling the "/.." case:
      // []      + "/.."  ==> []
      // [.]     + "/.."  ==> [./..]
      // [foo]   is always [./foo] due to init above
      // [*/..]  + "/.."  ==> [*/../..]
      // [*/foo] + "/.."  ==> [*]
      auto goParent_f =  [&](){
        if ( _name.empty() )
          /*NOOP*/;
        else if ( _name.size() == 1 ) // content is '.'
          _name += "/..";
        else
        {
          std::string::size_type pos = _name.rfind( '/' );
          if ( pos == _name.size() - 3 && _name[pos+1] == '.' && _name[pos+2] == '.' )
            _name += "/..";
          else
            _name.erase( pos );
        }
      };

      for ( char ch : name_r )
      {
        switch ( ch )
        {
          case '/':
            switch ( pending )
            {
              case P_none:	pending = P_slash; break;
              case P_slash:	break;
              case P_dot1:	pending = P_slash; break;
              case P_dot2:	goParent_f(); pending = P_slash; break;
            }
            break;

          case '.':
            switch ( pending )
            {
              case P_none:	_name += '.'; break;
              case P_slash:	pending = P_dot1; break;
              case P_dot1:	pending = P_dot2; break;
              case P_dot2:	_name += "/..."; pending = P_none; break;
            }
            break;

          default:
            switch ( pending )
            {
              case P_none:	break;
              case P_slash:	_name += '/';	 pending = P_none; break;
              case P_dot1:	_name += "/.";	 pending = P_none; break;
              case P_dot2:	_name += "/.."; pending = P_none; break;
            }
            _name += ch;
            break;
        }
      }

      switch ( pending )
      {
        case P_none:	break;
        case P_slash:	if ( _name.empty() ) _name = "/"; break;
        case P_dot1:	if ( _name.empty() ) _name = "/"; break;
        case P_dot2:	goParent_f(); if ( _name.empty() ) _name = "/"; break;
      }
      return;
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : Pathname::dirname
    //	METHOD TYPE : Pathname
    //
    Pathname Pathname::dirname( const Pathname & name_r )
    {
      if ( name_r.empty() )
        return Pathname();

      Pathname ret_t( name_r );
      std::string::size_type idx = ret_t._name.find_last_of( '/' );

      if ( idx == std::string::npos ) {
        ret_t._name = ".";
      } else if ( idx == 0 ) {
        ret_t._name = "/";
      } else {
        ret_t._name.erase( idx );
      }

      return ret_t;
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : Pathname::basename
    //	METHOD TYPE : string
    //
    string Pathname::basename( const Pathname & name_r )
    {
      if ( name_r.empty() )
        return string();

      string ret_t( name_r.asString() );
      std::string::size_type idx = ret_t.find_last_of( '/' );
      if ( idx != std::string::npos && ( idx != 0 || ret_t.size() != 1 ) ) {
        ret_t.erase( 0, idx+1 );
      }

      return ret_t;
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : Pathname::asUrl
    //	METHOD TYPE : Url
    //
    Url Pathname::asUrl( const std::string & scheme_r ) const
    {
      Url ret;
      ret.setPathName( asString() );
      ret.setScheme( scheme_r );
      return ret;
    }

    Url Pathname::asUrl() const
    { return asUrl( "dir" ); }

    Url Pathname::asDirUrl() const
    { return asUrl( "dir" ); }

    Url Pathname::asFileUrl() const
    { return asUrl( "file" ); }


    std::string Pathname::showRoot( const Pathname & root_r, const Pathname & path_r )
    {
      return str::Str() << "(" << root_r << ")" << path_r;
    }

    std::string Pathname::showRootIf( const Pathname & root_r, const Pathname & path_r )
    {
      if ( root_r.empty() || root_r == "/" )
        return path_r.asString();
      return showRoot( root_r, path_r );
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : Pathname::extension
    //	METHOD TYPE : string
    //
    string Pathname::extension( const Pathname & name_r )
    {
      if ( name_r.empty() )
        return string();

      string base( basename( name_r ) );
      std::string::size_type pos = base.rfind( '.' );
      switch ( pos )
      {
        case 0:
          if ( base.size() == 1 )			// .
            return string();
          break;
        case 1:
          if ( base.size() == 2 && base[0] == '.' )	// ..
            return string();
          break;
        case std::string::npos:
          return string();
          break;
      }
      return base.substr( pos );
    }

    Pathname Pathname::realpath() const
    {
      std::string real;
      if( !empty())
      {
    #if __GNUC__ > 2
        /** GNU extension */
        char *ptr = ::realpath(_name.c_str(), NULL);
        if( ptr != NULL)
        {
          real = ptr;
          free( ptr);
        }
        else
        /** the SUSv2 way */
        if( EINVAL == errno)
        {
          char buff[PATH_MAX + 2];
          memset(buff, '\0', sizeof(buff));
          if( ::realpath(_name.c_str(), buff) != NULL)
          {
            real = buff;
          }
        }
    #else
        char buff[PATH_MAX + 2];
        memset(buff, '\0', sizeof(buff));
        if( ::realpath(_name.c_str(), buff) != NULL)
        {
          real = buff;
        }
    #endif
      }
      return zypp::Pathname(real);
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : Pathname::assertprefix
    //	METHOD TYPE : Pathname
    //
    Pathname Pathname::assertprefix( const Pathname & root_r, const Pathname & path_r )
    {
      if ( root_r.empty() || root_r == path_r )
        return path_r;

      if ( str::hasPrefix( path_r.asString(), root_r.asString() ) && path_r.asString()[root_r.asString().size()] == '/' ) {
        // NOTE: Pathological is root_r being "." (opt. followed by a sequence of "/.."):
        // Pathname is normalized and does not contain any embedded "..". If root_r is ".(/..)*',
        // then path_r could continue with a '..' and this way would escape root_r.
        if ( root_r.absolute() || root_r.asString().find_first_not_of( "./" ) != std::string::npos )
          return path_r;

        const std::string & pathbegin = path_r.asString().substr( root_r.asString().size()+1, 3 );
        if ( pathbegin[0] != '.' || pathbegin[1] != '.' || ( pathbegin[2] != '\0' && pathbegin[2] != '/' )  )
          return path_r;
      }

      // Treat path_r as if it was in "/", then prefix root_r
      return root_r / absolutename(path_r);
    }

    Pathname Pathname::stripprefix( const Pathname & root_r, const Pathname & path_r )
    {
      if ( root_r.emptyOrRoot() )
        return path_r;
      if ( root_r == path_r )
        return "/";
      std::string rest( str::stripPrefix( path_r.asString(), root_r.asString() ) );
      if ( rest[0] == '/' )	// needs to be a dir prefix!
        return rest;
      return path_r;
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : Pathname::cat
    //	METHOD TYPE : Pathname
    //
    Pathname Pathname::cat( const Pathname & name_r, const Pathname & add_tv )
    {
      if ( add_tv.empty() )
        return name_r;
      if ( name_r.empty() )
        return add_tv;

      string ret_ti( name_r._name );
      if( add_tv._name[0] != '/' )
        ret_ti += '/';
      return ret_ti + add_tv._name;
    }

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : Pathname::Extend
    //	METHOD TYPE : Pathname
    //
    Pathname Pathname::extend( const Pathname & l, const string & r )
    {
      return l.asString() + r;
    }

    /////////////////////////////////////////////////////////////////
  } // namespace filesystem
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
