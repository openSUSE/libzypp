/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/base/String.cc
 *
*/
#include <cstdio>
#include <cstdarg>

#include <iostream>

#include "zypp/base/String.h"
#include "zypp/base/LogTools.h"

#include "zypp/TriBool.h"

using std::string;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace str
  { /////////////////////////////////////////////////////////////////

    /******************************************************************
     **
     **      FUNCTION NAME : form
     **      FUNCTION TYPE : std::string
    */
    std::string form( const char * format, ... )
    {
      SafeBuf safe;

      va_list ap;
      va_start( ap, format );
      vasprintf( &safe._buf, format, ap );
      va_end( ap );

      return safe.asString();
    }

    /******************************************************************
     **
     **      FUNCTION NAME : strerror
     **      FUNCTION TYPE : std::string
    */
    std::string strerror( int errno_r )
    {
      return form( "(%d)%s", errno_r, ::strerror( errno_r ) );
    }

    /******************************************************************
     **
     **      FUNCTION NAME : strToTrue
     **      FUNCTION TYPE : bool
    */
    bool strToTrue( const C_Str & str )
    {
      std::string t( toLower( str ) );
      return(    t == "1"
              || t == "yes"
              || t == "true"
              || t == "on"
              || t == "+"
              || strtonum<long long>( str )
            );
    }

    /******************************************************************
     **
     **      FUNCTION NAME : strToFalse
     **      FUNCTION TYPE : bool
    */
    bool strToFalse( const C_Str & str )
    {
      std::string t( toLower( str ) );
      return ! (    t == "0"
                 || t == "no"
                 || t == "false"
                 || t == "off"
                 || t == "-"
               );
    }

    TriBool strToTriBool( const C_Str & str )	// from TriBool.h
    {
      if ( strToTrue( str ) )	return true;
      if ( !strToFalse( str ) )	return false;
      return indeterminate;
    }

    ///////////////////////////////////////////////////////////////////
    // Hexencode
    ///////////////////////////////////////////////////////////////////
    namespace {
      /** What's not decoded. */
      inline bool heIsAlNum( char ch )
      {
        return ( ( 'a' <= ch && ch <= 'z' )
               ||( 'A' <= ch && ch <= 'Z' )
               ||( '0' <= ch && ch <= '9' ) );
      }
      /** Hex-digit to number or -1. */
      inline int heDecodeCh( char ch )
      {
        if ( '0' <= ch && ch <= '9' )
          return( ch - '0' );
        if ( 'A' <= ch && ch <= 'F' )
          return( ch - 'A' + 10 );
        if ( 'a' <= ch && ch <= 'f' )
          return( ch - 'a' + 10 );
        return -1;
      }
    }

    std::string hexencode( const C_Str & str_r )
    {
      static const char *const hdig = "0123456789ABCDEF";
      std::string res;
      res.reserve( str_r.size() );
      for ( const char * it = str_r.c_str(); *it; ++it )
      {
        if ( heIsAlNum( *it ) )
        {
          res += *it;
        }
        else
        {
          res += '%';
          res += hdig[(unsigned char)(*it)/16];
          res += hdig[(unsigned char)(*it)%16];
        }
      }
      return res;
    }

    std::string hexdecode( const C_Str & str_r )
    {
      std::string res;
      res.reserve( str_r.size() );
      for_( it, str_r.c_str(), str_r.c_str()+str_r.size() )
      {
        if ( *it == '%' )
        {
          int d1 = heDecodeCh( *(it+1) );
          if ( d1 != -1 )
          {
            int d2 = heDecodeCh( *(it+2) );
            if ( d2 != -1 )
            {
              res += (d1<<4)|d2;
              it += 2;
              continue;
            }
          }
        }
        // verbatim if no %XX:
        res += *it;
      }
      return res;
    }
    ///////////////////////////////////////////////////////////////////

    /******************************************************************
     **
     **      FUNCTION NAME : toLower
     **      FUNCTION TYPE : std::string
    */
    std::string toLower( const std::string & s )
    { return toLower( std::string(s) ); }

    std::string toLower( std::string && s )
    {
      std::string ret( std::move(s) );

      if ( ret.empty() )
        return ret;

      for ( std::string::size_type i = 0; i < ret.length(); ++i )
        {
          if ( isupper( ret[i] ) )
            ret[i] = static_cast<char>(tolower( ret[i] ));
        }
      return ret;
    }

    /******************************************************************
     **
     **      FUNCTION NAME : toUpper
     **      FUNCTION TYPE : std::string
    */
    std::string toUpper( const std::string & s )
    { return toUpper( std::string(s) ); }

    std::string toUpper( std::string && s )
    {
      std::string ret( std::move(s) );

      if ( ret.empty() )
        return ret;

      for ( std::string::size_type i = 0; i < ret.length(); ++i )
        {
          if ( islower( ret[i] ) )
            ret[i] = static_cast<char>(toupper( ret[i] ));
        }
      return ret;
    }

    /******************************************************************
     **
     **      FUNCTION NAME : trim
     **      FUNCTION TYPE : std::string
    */
    std::string trim( const std::string & s, const Trim trim_r )
    { return trim( std::string(s), trim_r ); }

    std::string trim( std::string && s, const Trim trim_r )
    {
      std::string ret( std::move(s) );

      if ( ret.empty() || trim_r == NO_TRIM )
        return ret;

      if ( trim_r & L_TRIM )
      {
	std::string::size_type p = ret.find_first_not_of( " \t\n" );
	if ( p == std::string::npos )
	{
	  ret.clear();
	  return ret;
	}
	ret.erase( 0, p );
      }

      if ( trim_r & R_TRIM )
      {
	std::string::size_type p = ret.find_last_not_of( " \t\n" );
	if ( p == std::string::npos )
	{
	  ret.clear();
	  return ret;
	}
	ret = ret.erase( p+1 );
      }

      return ret;
    }

    /******************************************************************
    **
    **	FUNCTION NAME : stripFirstWord
    **	FUNCTION TYPE : std::string
    */
    std::string stripFirstWord( std::string & line, const bool ltrim_first )
    {
      if ( ltrim_first )
        line = ltrim( line );

      if ( line.empty() )
        return line;

      std::string ret;
      std::string::size_type p = line.find_first_of( " \t" );

      if ( p == std::string::npos ) {
        // no ws on line
        ret = line;
        line.erase();
      } else if ( p == 0 ) {
        // starts with ws
        // ret remains empty
        line = ltrim( line );
      }
      else {
        // strip word and ltim line
        ret = line.substr( 0, p );
        line = ltrim( line.erase( 0, p ) );
      }
      return ret;
    }

    /******************************************************************
    **
    **	FUNCTION NAME : stripLastWord
    **	FUNCTION TYPE : std::string
    */
    std::string stripLastWord( std::string & line, const bool rtrim_first )
    {
      if ( rtrim_first )
        line = rtrim( line );

      if ( line.empty() )
        return line;

      std::string ret;
      std::string::size_type p = line.find_last_of( " \t" );

      if ( p == std::string::npos ) {
        // no ws on line
        ret = line;
        line.erase();
      } else if ( p == line.size()-1 ) {
        // ends with ws
        // ret remains empty
        line = rtrim( line );
      }
      else {
        // strip word and rtim line
        ret = line.substr( p+1 );
        line = rtrim( line.erase( p ) );
      }
      return ret;
    }

    std::string gsub( const std::string & str_r, const std::string & from_r, const std::string & to_r )
    {
      std::string ret( str_r );
      return replaceAll( ret, from_r, to_r );
    }

    std::string & replaceAll( std::string & str_r, const std::string & from_r, const std::string & to_r )
    {
      if ( ! from_r.empty() )
      {
	std::string::size_type pos = 0;
	while ( (pos = str_r.find( from_r, pos )) != std::string::npos )
	{
	  str_r.replace( pos, from_r.size(), to_r );
	  pos += to_r.size();

	  if ( pos >= str_r.length() )
	    break;
	}
      }
      return str_r;
    }

    std::string gsubFun( const std::string & str_r, const std::string & from_r, function<std::string()> to_r )
    {
      std::string ret( str_r );
      return replaceAllFun( ret, from_r, to_r );
    }

    std::string & replaceAllFun( std::string & str_r, const std::string & from_r, function<std::string()> to_r )
    {
      if ( ! from_r.empty() )
      {
	std::string::size_type pos = 0;
	while ( (pos = str_r.find( from_r, pos )) != std::string::npos )
	{
	  std::string to( to_r() );
	  str_r.replace( pos, from_r.size(), to );
	  pos += to.size();

	  if ( pos >= str_r.length() )
	    break;
	}
      }
      return str_r;
    }

    std::string escape( const C_Str & str_r, const char sep_r )
    {
      std::vector<char> buf;
      for_( s, str_r.c_str(), s+str_r.size() )
      {
        switch ( *s )
        {
        case '"':
        case '\'':
        case '\\':
          buf.push_back( '\\' );
          buf.push_back( *s );
          break;
        default:
          if ( *s == sep_r )
            buf.push_back( '\\' );
          buf.push_back( *s );
        }
      }
      return std::string( buf.begin(), buf.end() );
    }


    std::string bEscape( std::string str_r, const C_Str & special_r )
    {
      if ( str_r.empty() )
	return str_r;

      if ( str_r.find_first_of( special_r ) == std::string::npos
	&& ( ::strchr( special_r.c_str(), '\\' ) ||  !::strchr( str_r.c_str(), '\\' ) ) )
	return str_r;

      Str buf;
      for_( s, str_r.c_str(), s+str_r.size() )
      {
	if ( *s == '\\' || ::strchr( special_r.c_str(), *s ) )
	  buf << '\\';
	buf << *s;
      }
      return buf;
    }

    #define RXSPECIALCHARS "\\.*+?^$[()|{"

    std::string rxEscapeStr( std::string str_r )
    {
      return bEscape( std::move(str_r), RXSPECIALCHARS );
    }

    std::string rxEscapeGlob( std::string str_r )
    {
      if ( str_r.empty() )
	return str_r;

      if ( str_r.find_first_of( RXSPECIALCHARS ) == std::string::npos )
	return str_r;

      Str buf;
      for_( s, str_r.c_str(), s+str_r.size() )
      {
	if ( *s == '\\' )	// + next char literally
	{
	  buf << '\\';
	  if ( *(s+1) ) { ++s; buf << *s; }
	}
	else if ( *s == '?' )	// translate
	{
	  buf << '.';
	}
	else if ( *s == '*' )	// translate
	{
	  buf << ".*";
	}
	else if ( *s == '[' )	// character class if closing ] is found, else literally
	{
	  const char * e = s+1;
	  if ( *e == '^' || *e == '!' )	// negated cclass
	    ++e;
	  if ( *e == ']' )		// ] in cclass
	    ++e;
	  while ( *e && *e != ']' )	// ...to ] or \0
	    ++e;
	  if ( *e ) // on closing ']'
	  {
	    ++s;  buf << '[' << (*s == '!' ? '^' : *s );
	    while ( ++s != e )
	      buf << *s;
	    buf << ']';
	  }
	  else
	  {
	    buf << "\\[";
	  }
	}
	else if ( ::strchr( RXSPECIALCHARS, *s ) )	// escape
	{
	  buf << '\\' << *s;
	}
	else
	{
	  buf << *s;
	}
      }
      return buf;
    }


    std::string getline( std::istream & str, const Trim trim_r )
    {
      return trim( receiveUpTo( str, '\n' ), trim_r );
    }

    std::string getline( std::istream & str, bool trim_r )
    {
      return trim( receiveUpTo( str, '\n' ), trim_r?TRIM:NO_TRIM );
    }

    std::string receiveUpTo( std::istream & str, const char delim_r, bool returnDelim_r )
    {
      std::ostringstream datas;
      do {
	char ch;
	if ( str.get( ch ) )
	{
	  if ( ch != delim_r )
	  {
	    datas.put( ch );
	  }
	  else
	  {
	    if ( returnDelim_r )
	      datas.put( ch );
	    break;	// --> delimiter found
	  }
	}
	else
	{
	  // clear fail bit if we read data before reaching EOF
	  if ( str.eof() && datas.tellp() )
	    str.clear( std::ios::eofbit );
	  break;	// --> no consumable data.
	}
      } while ( true );
      return datas.str();
    }

    /////////////////////////////////////////////////////////////////
  } // namespace str
  ///////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////
} // namespace zypp
//////////////////////////////////////////////////////////////////
