/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-core/ng/config/configparsers.h
 *
 * Parse/serialize helpers for the core value types used by config keys
 * (bool, long, unsigned, string, Pathname, string set, TriBool).
 *
 * Application-layer types (Arch, Locale, ...) have their parsers in
 * zypp/ng/config/zyppconfig.h.
 */
#ifndef ZYPP_CORE_NG_CONFIG_CONFIGPARSERS_H
#define ZYPP_CORE_NG_CONFIG_CONFIGPARSERS_H

#include <set>
#include <string>
#include <string_view>

#include <zypp-core/Pathname.h>
#include <zypp-core/TriBool.h>
#include <zypp-core/ng/pipelines/expected.h>

namespace zyppng::config {

  // ---------------------------------------------------------------------------
  // Parse helpers — return expected<T>; on failure the error holds a std::exception_ptr.
  // ---------------------------------------------------------------------------

  expected<bool>                      parseBool          ( std::string_view s );
  /**
   * Parse an integer like legacy str::strtonum (strtol, base 0): optional sign,
   * surrounding whitespace, decimal, hex ("0x10") and octal ("010" == 8).
   * Errors (the key falls back to its default): no digits, trailing garbage,
   * out of range. Legacy would silently use 0 / the numeric prefix instead.
   */
  expected<long>                      parseLong          ( std::string_view s );
  /** Like parseLong() (strtoul, base 0), but a leading '-' and values above UINT_MAX are errors. */
  expected<unsigned>                  parseUnsigned      ( std::string_view s );
  expected<std::string>               parseString        ( std::string_view s );
  expected<zypp::TriBool>             parseTriBool       ( std::string_view s );
  expected<std::set<std::string>>     parseStringSet     ( std::string_view s );  ///< splits on ", \t"
  /**
   * Parse a filesystem path from a config value.
   * zypp::Pathname normalises on construction; a relative path that still
   * starts with "./.." (relativeDotDot()) could escape a chroot root when
   * composed with root() and is rejected.
   * Absolute paths and paths like "some/../dir" (normalises to "./dir") are accepted.
   */
  expected<zypp::Pathname>            parsePath          ( std::string_view s );

  // ---------------------------------------------------------------------------
  // Serialize helpers — must round-trip with their corresponding parse helpers.
  // ---------------------------------------------------------------------------

  std::string serializeBool       ( const bool & v );
  std::string serializeLong       ( const long & v );
  std::string serializeUnsigned   ( const unsigned & v );
  std::string serializeString     ( const std::string & v );
  std::string serializePath       ( const zypp::Pathname & v );
  std::string serializeTriBool    ( const zypp::TriBool & v );
  std::string serializeStringSet  ( const std::set<std::string> & v );  ///< escapes separators/quotes; round-trips with parseStringSet

} // namespace zyppng::config

#endif // ZYPP_CORE_NG_CONFIG_CONFIGPARSERS_H
