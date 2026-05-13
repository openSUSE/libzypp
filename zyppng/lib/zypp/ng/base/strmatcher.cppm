/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::StrMatcher into zyppng namespace.
// The operator overloads are ADL wrappers — kept as inline definitions
// because the legacy types live in zypp:: and ADL would not find them
// through a plain using-declaration in zyppng::.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <iosfwd>
#include <zypp/base/StrMatcher.h>

export module zyppng:base_strmatcher;

export namespace zyppng {
  using ::zypp::Match;
  using ::zypp::MatchException;
  using ::zypp::MatchUnknownModeException;
  using ::zypp::MatchInvalidRegexException;
  using ::zypp::StrMatcher;

  // ADL wrappers: route operators back to zypp:: for correct resolution.
  inline bool operator==( const Match & lhs, const Match & rhs )
  { return ::zypp::operator==( lhs, rhs ); }

  inline bool operator!=( const Match & lhs, const Match & rhs )
  { return ::zypp::operator!=( lhs, rhs ); }

  inline Match operator|( const Match & lhs, const Match & rhs )
  { return ::zypp::operator|( lhs, rhs ); }

  inline Match operator-( const Match & lhs, const Match & rhs )
  { return ::zypp::operator-( lhs, rhs ); }

  inline std::ostream & operator<<( std::ostream & str, Match::Mode obj )
  { return ::zypp::operator<<( str, obj ); }

  inline bool operator<( const StrMatcher & lhs, const StrMatcher & rhs )
  { return ::zypp::operator<( lhs, rhs ); }
} // namespace zyppng
