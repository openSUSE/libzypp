/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include <zypp-core/ng/config/configparsers.h>

#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>

#include <zypp-core/base/String.h>

namespace zyppng::config {

  // ---------------------------------------------------------------------------
  // Parse helpers
  // ---------------------------------------------------------------------------

  expected<bool> parseBool( std::string_view s )
  {
    try {
      // strToTriBool returns indeterminate for unrecognised values —
      // unambiguous unlike strToBoolNodefault which returns false for both
      // "parsed as false" and "could not parse".
      zypp::TriBool result = zypp::str::strToTriBool( std::string(s).c_str() );
      if ( zypp::indeterminate(result) )
        throw std::invalid_argument( "not a boolean value: '" + std::string(s) + "'" );
      return bool(result);
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<long> parseLong( std::string_view s )
  {
    try {
      std::size_t pos = 0;
      long val = std::stol( std::string(s), &pos );
      if ( pos != s.size() )
        throw std::invalid_argument( "trailing characters in long value: '" + std::string(s) + "'" );
      return val;
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<unsigned> parseUnsigned( std::string_view s )
  {
    try {
      std::size_t pos = 0;
      unsigned long val = std::stoul( std::string(s), &pos );
      if ( pos != s.size() )
        throw std::invalid_argument( "trailing characters in unsigned value: '" + std::string(s) + "'" );
      if ( val > std::numeric_limits<unsigned>::max() )
        throw std::out_of_range( "value out of range for unsigned: '" + std::string(s) + "'" );
      return static_cast<unsigned>(val);
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<std::string> parseString( std::string_view s )
  {
    return std::string(s);
  }

  expected<zypp::TriBool> parseTriBool( std::string_view s )
  {
    try {
      bool val = false;
      if ( zypp::str::strToBoolNodefault( std::string(s).c_str(), val ) )
        return zypp::TriBool(val);
      // indeterminate is not an error — it is a valid config value meaning
      // "defer to the master gpgcheck switch".
      return zypp::TriBool(zypp::indeterminate);
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<std::set<std::string>> parseStringSet( std::string_view s )
  {
    try {
      std::vector<std::string> tokens;
      zypp::str::splitEscaped( std::string(s).c_str(), std::back_inserter(tokens), ", \t" );
      return std::set<std::string>( tokens.begin(), tokens.end() );
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<zypp::Pathname> parsePath( std::string_view s )
  {
    try {
      // Callers build the effective path as Config::root() / value. Pathname
      // normalises on construction, so a relative value that still starts with
      // "./.." would resolve above root() (e.g. "/mnt" / "../x" -> "/x") and
      // leave the chroot. Reject it here. Absolute values are safe: Pathname's
      // operator/ appends them below root() ("/mnt" / "/etc" -> "/mnt/etc").
      zypp::Pathname p { std::string(s) };
      if ( p.relativeDotDot() )
        throw std::invalid_argument(
          "path would escape root after normalisation: '" + std::string(s) + "'" );
      return p;
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  // ---------------------------------------------------------------------------
  // Serialize helpers
  // ---------------------------------------------------------------------------

  std::string serializeBool( const bool & v )
  { return v ? "true" : "false"; }

  std::string serializeLong( const long & v )
  { return std::to_string(v); }

  std::string serializeUnsigned( const unsigned & v )
  { return std::to_string(v); }

  std::string serializeString( const std::string & v )
  { return v; }

  std::string serializePath( const zypp::Pathname & v )
  { return v.asString(); }

  std::string serializeTriBool( const zypp::TriBool & v )
  {
    if ( zypp::indeterminate(v) ) return "";
    return bool(v) ? "true" : "false";
  }

  std::string serializeStringSet( const std::set<std::string> & v )
  {
    std::string result;
    for ( const auto & s : v ) {
      if ( !result.empty() ) result += ", ";
      result += s;
    }
    return result;
  }

} // namespace zyppng::config
