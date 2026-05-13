/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::RelCompare into zyppng namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <zypp/RelCompare.h>

export module zyppng:relcompare;

export namespace zyppng {
  using ::zypp::Compare;
  using ::zypp::compareByRel;
  using ::zypp::CompareBy;
  using ::zypp::CompareByEQ;
  using ::zypp::CompareByNE;
  using ::zypp::CompareByLT;
  using ::zypp::CompareByLE;
  using ::zypp::CompareByGT;
  using ::zypp::CompareByGE;
  using ::zypp::CompareByANY;
  using ::zypp::CompareByNONE;
} // namespace zyppng
