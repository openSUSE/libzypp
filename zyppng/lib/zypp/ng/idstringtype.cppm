/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::IdStringType into zyppng namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <zypp/IdStringType.h>

export module zyppng:idstringtype;

export namespace zyppng {
  using ::zypp::IdStringType;
  using ::zypp::operator<<;
  using ::zypp::operator==;
  using ::zypp::operator!=;
  using ::zypp::operator<;
  using ::zypp::operator<=;
  using ::zypp::operator>;
  using ::zypp::operator>=;
} // namespace zyppng
