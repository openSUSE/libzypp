/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::bit into zyppng::bit namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <zypp/Bit.h>

export module zyppng:bit;

export namespace zyppng::bit {
  using ::zypp::bit::MaxBits;
  using ::zypp::bit::asString;
  using ::zypp::bit::Mask;
  using ::zypp::bit::Range;
  using ::zypp::bit::RangeValue;
  using ::zypp::bit::RangeBit;
  using ::zypp::bit::BitField;
  using ::zypp::bit::operator<<;
  using ::zypp::bit::operator==;
  using ::zypp::bit::operator!=;
  using ::zypp::bit::operator&;
  using ::zypp::bit::operator|;
  using ::zypp::bit::operator^;
  using ::zypp::bit::operator>>;
} // namespace zyppng::bit
