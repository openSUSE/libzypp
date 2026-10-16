/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* Strictly for internal use!
*/

#ifndef ZYPP_TUI_OUTPUT_UTF8_H_
#define ZYPP_TUI_OUTPUT_UTF8_H_

#include <algorithm>
#include <iostream>
#include <string>
#include <utility>

#include <zypp-tui/utils/text.h>

namespace ztui {

///////////////////////////////////////////////////////////////////
namespace utf8
{
  /** Simple utf8 string */
  class string
  {
  public:
    using size_type = std::string::size_type;
    static const size_type npos = std::string::npos;

  public:
    string() {}
    string( const char * rhs )		: _str( rhs ) {}
    string( std::string  rhs )	: _str(std::move( rhs )) {}

  public:
    const char * c_str() const			{ return _str.c_str(); }
    const std::string & str() const		{ return _str; }
    std::string & str()				{ return _str; }

  public:
    /** Terminal column width, ignoring ANSI SGR sequences. */
    size_type size() const
    {
      size_type ret = 0;
      for ( mbs::MbsIteratorNoSGR it( _str ); !it.atEnd(); ++it )
        ret += it.columns();
      return ret;
    }

    /** \overload std::string has both too */
    size_type length() const
    { return size(); }

    /** Substring by terminal columns, padding clipped wide characters. */
    string substr( size_type pos_r = 0, size_type len_r = npos ) const
    {
      std::string ret;
      if ( !len_r || pos_r == npos )
        return string( std::move( ret ) );

      const size_type colend = ( len_r > npos-pos_r ? npos : pos_r+len_r );
      mbs::MbsIteratorNoSGR it( _str );
      // Construction skips leading SGR; a substring from zero includes them.
      if ( !pos_r )
        ret.append( _str.data(), it.pos()-_str.data() );

      size_type pos = 0;
      while ( !it.atEnd() && pos < colend )
      {
        const size_type end = pos + it.columns();
        const char * cpos = it.pos();
        const char * cend = cpos + it.size();
        ++it;
        // The byte span includes SGR skipped while advancing the iterator.
        if ( pos >= pos_r && end <= colend )
          ret.append( cpos, it.pos()-cpos );
        else if ( end > pos_r && pos < colend )
        {
          ret.append( std::min( end, colend )-std::max( pos, pos_r ), ' ' );
          ret.append( cend, it.pos()-cend );
        }
        pos = end;
      }
      return string( std::move( ret ) );
    }

  private:
    std::string _str;
  };

  /** relates: string concatenation */
  inline string operator+( const string & lhs, const string & rhs )
  { return string( lhs.str() + rhs.str() ); }
  /** \overload */
  inline string operator+( const string & lhs, const std::string & rhs )
  { return string( lhs.str() + rhs ); }
  /** \overload */
  inline string operator+( const std::string & lhs, const string & rhs )
  { return string( lhs + rhs.str() ); }
  /** \overload */
  inline string operator+( const string & lhs, const char * rhs )
  { return string( lhs.str() + rhs ); }
  /** \overload */
  inline string operator+( const char * lhs, const string & rhs )
  { return string( lhs + rhs.str() ); }

  /** relates: string Stream output */
  inline std::ostream & operator<<( std::ostream & str, const string & obj )
  { return str << obj.str(); }

} // namespace utf8
///////////////////////////////////////////////////////////////////

}

#endif // ZYPP_TUI_OUTPUT_UTF8_H_
