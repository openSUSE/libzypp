/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_CORE_PARSER_JSON_JSON_NUMBER_DEFINED
#define ZYPP_CORE_PARSER_JSON_JSON_NUMBER_DEFINED

#include <string>
#include <cstdint>
#include <ostream>

#include <zypp-core/Globals.h>

namespace zypp::json {

  class ZYPP_API Number {

  public:
    Number() = default;
    explicit Number( double v ) : _value(v) {}
    explicit Number( float v ) : _value(v) {}
    ~Number() = default;

    Number( const Number & ) = default;
    Number( Number && ) = default;
    Number &operator=(const Number &) = default;
    Number &operator=(Number &&) = default;

    operator double() const {
      return _value;
    }

    double value() const {
      return _value;
    }

    /** JSON representation */
    std::string asJSON() const
    { return std::to_string (_value); }

    /** Stream output */
    std::ostream & dumpOn( std::ostream & str ) const
    { return str << asJSON(); }

  private:
    double _value = 0;

  };

  /** relates: Number Stream output */
  inline ZYPP_API std::ostream & operator<<( std::ostream & str, const Number & obj )
  {
    return obj.dumpOn( str );
  }

  class ZYPP_API Int {

  public:
    Int() = default;
    Int( std::int64_t v ) : _value(v) {}
    ~Int() = default;

    Int( const Int & ) = default;
    Int( Int && ) = default;
    Int &operator=(const Int &) = default;
    Int &operator=(Int &&) = default;

    operator std::int64_t() const {
      return _value;
    }

    std::int64_t value() const {
      return _value;
    }

    /** JSON representation */
    std::string asJSON() const
    { return std::to_string (_value); }

    /** Stream output */
    std::ostream & dumpOn( std::ostream & str ) const
    { return str << asJSON(); }

  private:
    std::int64_t _value = 0;
  };

  /** relates: Int Stream output */
  inline std::ostream & operator<<( std::ostream & str, const Int & obj )
  {
    return obj.dumpOn( str );
  }

  class ZYPP_API UInt {

  public:
    UInt() = default;
    UInt( std::uint64_t v ) : _value(v) {}
    ~UInt() = default;

    UInt( const UInt & ) = default;
    UInt( UInt && ) = default;
    UInt &operator=(const UInt &) = default;
    UInt &operator=(UInt &&) = default;

    operator std::uint64_t() const {
      return _value;
    }

    std::uint64_t value() const {
      return _value;
    }

    /** JSON representation */
    std::string asJSON() const
    { return std::to_string (_value); }

    /** Stream output */
    std::ostream & dumpOn( std::ostream & str ) const
    { return str << asJSON(); }

  private:
    std::uint64_t _value = 0;
  };

  /** relates: UInt Stream output */
  inline std::ostream & operator<<( std::ostream & str, const UInt & obj )
  {
    return obj.dumpOn( str );
  }

}

#endif
