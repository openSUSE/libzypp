/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::IdString into zyppng namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <unordered_set>
#include <zypp/IdString.h>

export module zyppng:idstring;

export namespace zyppng {
  using ::zypp::IdString;
  using IdStringSet = std::unordered_set<IdString>;
  using ::zypp::operator<<;
  using ::zypp::dumpOn;
  using ::zypp::operator==;
  using ::zypp::operator!=;
  using ::zypp::operator<;
  using ::zypp::operator<=;
  using ::zypp::operator>;
  using ::zypp::operator>=;
} // namespace zyppng
