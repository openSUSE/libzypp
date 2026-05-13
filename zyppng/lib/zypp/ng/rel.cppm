/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::Rel into zyppng namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <zypp/Rel.h>

export module zyppng:rel;

export namespace zyppng {
  using ::zypp::Rel;
  using ::zypp::operator<<;
  using ::zypp::operator==;
  using ::zypp::operator!=;
} // namespace zyppng
