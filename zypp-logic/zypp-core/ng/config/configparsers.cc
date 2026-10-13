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
    // Exactly legacy ZConfig (repo_gpgcheck / pkg_gpgcheck): true token → true,
    // false token → false, anything else → indeterminate ("defer to the master
    // gpgcheck switch"). Unrecognised input is NOT an error.
    try {
      return zypp::str::strToTriBool( std::string(s).c_str() );
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
    // Must round-trip through parseStringSet (zypp::str::splitEscaped, separators
    // ", \t"). Outside quotes a backslash makes the next character literal, so
    // every separator and quoting character inside an element is backslash-escaped
    // (str::joinEscaped() is not usable: it only escapes ONE separator character).
    // An empty element can only be expressed as an empty quote pair ''.
    // Plain values ("download.opensuse.org") are emitted unchanged.
    std::string result;
    bool first = true;
    for ( const auto & s : v ) {
      if ( !first ) result += ", ";
      first = false;
      if ( s.empty() )
        result += "''";
      else
        result += zypp::str::bEscape( s, ", \t'\"" );   // also escapes '\\' itself
    }
    return result;
  }

} // namespace zyppng::config
