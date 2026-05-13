/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::Dep into zyppng namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <zypp/Dep.h>

export module zyppng:dep;

export namespace zyppng {
  using ::zypp::Dep;
  using ::zypp::operator<<;
  using ::zypp::operator==;
  using ::zypp::operator!=;
  using ::zypp::operator<;
} // namespace zyppng
