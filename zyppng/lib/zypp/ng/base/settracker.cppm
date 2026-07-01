/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::base::SetTracker into zyppng::base namespace.
// The operator<< is an ADL wrapper template — kept as a definition rather
// than a using-declaration because the template parameter prevents plain
// using from resolving correctly across namespaces.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <iosfwd>
#include <zypp/base/SetTracker.h>

export module zyppng:base_settracker;

export namespace zyppng::base {
  using ::zypp::base::SetTracker;

  // ADL wrapper: routes operator<< back to zypp::base for correct resolution.
  template <typename TSet>
  inline std::ostream & operator<<( std::ostream & str, const SetTracker<TSet> & obj )
  { return ::zypp::base::operator<<( str, obj ); }
} // namespace zyppng::base
