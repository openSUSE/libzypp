/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include <zypp-core/ng/config/configparsers.h>

#include <cctype>
#include <cerrno>
#include <cstdlib>
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
    // Accepts every form legacy str::strtonum (strtol, base 0) accepts: sign,
    // leading/trailing whitespace, hex "0x10", octal "010" (= 8).
    // Unlike legacy, these are errors (the key falls back to its default):
    // no digits, trailing garbage ("12abc"), out of range.
    try {
      const std::string str( s );
      const char * begin = str.c_str();
      const char * stop  = begin + str.size();   // not '\0': an embedded NUL must not end the check
      char * end = nullptr;
      errno = 0;
      const long val = std::strtol( begin, &end, 0 );
      if ( end == begin )
        throw std::invalid_argument( "not a number: '" + str + "'" );
      if ( errno == ERANGE )
        throw std::out_of_range( "number out of range: '" + str + "'" );
      while ( end < stop && std::isspace( static_cast<unsigned char>(*end) ) )
        ++end;
      if ( end != stop )
        throw std::invalid_argument( "trailing characters in number: '" + str + "'" );
      return val;
    } catch (...) {
      return unexpected( std::current_exception() );
    }
  }

  expected<unsigned> parseUnsigned( std::string_view s )
  {
    // Same accepted forms as parseLong (strtoul, base 0), with two additions:
    // a leading '-' is an error (strtoul would silently wrap "-1" around to a
    // huge value — legacy has that bug), and values above UINT_MAX are errors.
    try {
      const std::string str( s );
      const char * begin = str.c_str();
      const char * stop  = begin + str.size();
      // strtol/strtoul skip leading whitespace themselves, so this loop does not
      // consume anything: it only locates the first significant character to
      // detect a sign. strtoul() below still parses from 'begin'.
      const char * first = begin;
      while ( first < stop && std::isspace( static_cast<unsigned char>(*first) ) )
        ++first;
      if ( first < stop && *first == '-' )
        throw std::invalid_argument( "negative value for unsigned: '" + str + "'" );

      char * end = nullptr;
      errno = 0;
      const unsigned long val = std::strtoul( begin, &end, 0 );
      if ( end == begin )
        throw std::invalid_argument( "not a number: '" + str + "'" );
      if ( errno == ERANGE || val > std::numeric_limits<unsigned>::max() )
        throw std::out_of_range( "value out of range for unsigned: '" + str + "'" );
      while ( end < stop && std::isspace( static_cast<unsigned char>(*end) ) )
        ++end;
      if ( end != stop )
        throw std::invalid_argument( "trailing characters in number: '" + str + "'" );
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
