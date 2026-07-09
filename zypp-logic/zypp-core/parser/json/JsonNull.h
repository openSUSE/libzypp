/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_CORE_PARSER_JSON_JSON_NULL_DEFINED
#define ZYPP_CORE_PARSER_JSON_JSON_NULL_DEFINED

#include <string_view>
#include <string>
#include <ostream>

#include <zypp-core/Globals.h>

namespace zypp::json {

  static constexpr std::string_view nullJSON ("null");

  class ZYPP_API Null {

  public:
    Null() = default;
    Null(std::nullptr_t){}

    ~Null() = default;

    Null( const Null & ) = default;
    Null( Null && ) = default;
    Null &operator=(const Null &) = default;
    Null &operator=(Null &&) = default;

    /** JSON representation */
    std::string asJSON() const
    { return std::string(nullJSON); }

    /** Stream output */
    std::ostream & dumpOn( std::ostream & str ) const
    { return str << nullJSON; }

    // comparison
    bool operator==( const Null & ) const { return true; }

  };

  /** relates: Null Stream output */
  inline ZYPP_API std::ostream & operator<<( std::ostream & str, const Null & obj )
  {
    return obj.dumpOn( str );
  }
}

#endif
