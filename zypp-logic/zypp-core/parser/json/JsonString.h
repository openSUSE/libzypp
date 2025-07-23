/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_CORE_PARSER_JSON_JSON_STRING_DEFINED
#define ZYPP_CORE_PARSER_JSON_JSON_STRING_DEFINED

#include <string>

namespace zypp::json {

  namespace detail
  {
    inline std::string strEncode( std::string val_r )
    {
      typedef unsigned char uchar;

      std::string::size_type add = 2;	// enclosing "s
      for( const auto &r : val_r )
      {
        if ( uchar(r) < 32u )
        {
          switch ( r )
          {
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
              add += 1;	// "\c"
              break;
            default:
              add += 5;	// "\uXXXX"
              break;
          }
        }
        else
        {
          switch ( r )
          {
            case '"':
            case '/':
            case '\\':
              add += 1;	// \-escape
              break;
          }
        }
      }

      val_r.resize( val_r.size() + add, '@' );
      auto w( val_r.rbegin() );
      auto r( w + add );

      *w++ = '"';
      for ( ; r != val_r.rend(); ++r )
      {
        if ( uchar(*r) < 32u )
        {
          static const char * digit = "0123456789abcdef";
          switch ( *r )
          {
            case '\b':	// "\c"
              *w++ = 'b';
              *w++ = '\\';
              break;
            case '\f':	// "\c"
              *w++ = 'f';
              *w++ = '\\';
              break;
            case '\n':	// "\c"
              *w++ = 'n';
              *w++ = '\\';
              break;
            case '\r':	// "\c"
              *w++ = 'r';
              *w++ = '\\';
              break;
            case '\t':	// "\c"
              *w++ = 't';
              *w++ = '\\';
              break;
            default:		// "\uXXXX"
              *w++ = digit[uchar(*r) % 15];
              *w++ = digit[uchar(*r) / 16];
              *w++ = '0';
              *w++ = '0';
              *w++ = 'u';
              *w++ = '\\';
              break;
          }
        }
        else
        {
          switch ( (*w++ = *r) )
          {
            case '"':
            case '/':
            case '\\':	// \-escape
              *w++ = '\\';
              break;
          }
        }
      }
      *w++ = '"';
      return val_r;
    }
  } // namespace detail

  class String {

  public:
    String() = default; //default false
    ~String() = default;

    String( const std::string &val ) : _value(val) { }
    String( std::string &&val ) : _value( std::move(val) ) { }

    String( std::nullptr_t ) : _value( "" ) {}

    String( const char * val_r )	: _value( val_r ? val_r : "" ) {}

    String( const String & ) = default;
    String( String && ) = default;
    String &operator=(const String &) = default;
    String &operator=(String &&) = default;

    String &operator=(const std::string &set ) {
      _value = set;
      return *this;
    }

    operator std::string() const {
      return _value;
    }

    /** JSON representation */
    std::string asJSON() const
    { return detail::strEncode(_value); }

    /** Stream output */
    std::ostream & dumpOn( std::ostream & str ) const
    { return str << asJSON(); }

    bool operator< ( const String &other ) const {
      return _value < other._value;
    }

    friend bool operator== ( const String &lhs, const String &rhs ) {
      return lhs._value == rhs._value;
    }

  private:
    std::string _value;

  };

  /** \relates String Stream output */
  inline std::ostream & operator<<( std::ostream & str, const String & obj )
  {
    return obj.dumpOn( str );
  }

}
#endif
