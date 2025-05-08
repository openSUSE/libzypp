/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_CORE_PARSER_JSON_JSON_BOOL_DEFINED
#define ZYPP_CORE_PARSER_JSON_JSON_BOOL_DEFINED

#include <string_view>
#include <string>

namespace zypp::json {

  static constexpr std::string_view trueJSON("true");
  static constexpr std::string_view falseJSON("false");

  class Bool {

  public:
    Bool() = default; //default false
    ~Bool() = default;

    Bool( bool val ) : _value(val) {}

    Bool( const Bool & ) = default;
    Bool( Bool && ) = default;
    Bool &operator=(const Bool &) = default;
    Bool &operator=(Bool &&) = default;

    Bool &operator=(bool set ) {
      _value = set;
      return *this;
    }

    operator bool() const {
      return _value;
    }

    /** JSON representation */
    std::string asJSON() const
    { return std::string(_value ? trueJSON : falseJSON); }

    /** Stream output */
    std::ostream & dumpOn( std::ostream & str ) const
    { return str << (_value ? trueJSON : falseJSON); }

  private:
    bool _value = false;

  };

  /** \relates Bool Stream output */
  inline std::ostream & operator<<( std::ostream & str, const Bool & obj )
  {
    return obj.dumpOn( str );
  }
}

#endif
