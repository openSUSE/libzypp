/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::Range into zyppng namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <zypp/Range.h>

export module zyppng:range;

export namespace zyppng::range_detail {
  using ::zypp::range_detail::overlaps;
} // namespace zyppng::range_detail

export namespace zyppng {
  using ::zypp::Range;
  using ::zypp::overlaps;
  using ::zypp::operator==;
  using ::zypp::operator!=;
} // namespace zyppng
