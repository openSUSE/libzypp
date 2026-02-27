/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// Automatically generated wrapper for zypp/base/StrMatcher.h
#ifndef STRMATCHER_H_ZYPPNG_WRAPPER_H
#define STRMATCHER_H_ZYPPNG_WRAPPER_H

#include <zypp/base/StrMatcher.h>

namespace zyppng
{
  using ::zypp::Match;
  using ::zypp::MatchException;
  using ::zypp::MatchUnknownModeException;
  using ::zypp::MatchInvalidRegexException;
  using ::zypp::StrMatcher;

  // ADL specific wrapper
  inline bool operator==(const Match & lhs, const Match & rhs) { return ::zypp::operator==(lhs, rhs); }

  // ADL specific wrapper
  inline bool operator!=(const Match & lhs, const Match & rhs) { return ::zypp::operator!=(lhs, rhs); }

  // ADL specific wrapper
  inline Match operator|(const Match & lhs, const Match & rhs) { return ::zypp::operator|(lhs, rhs); }

  // ADL specific wrapper
  inline Match operator-(const Match & lhs, const Match & rhs) { return ::zypp::operator-(lhs, rhs); }

  // ADL specific wrapper
  inline std::ostream & operator<<(std::ostream & str, Match::Mode obj) { return ::zypp::operator<<(str, obj); }

  // ADL specific wrapper
  inline bool operator<(const StrMatcher & lhs, const StrMatcher & rhs) { return ::zypp::operator<(lhs, rhs); }
} // namespace zyppng
#endif // STRMATCHER_H_ZYPPNG_WRAPPER_H
