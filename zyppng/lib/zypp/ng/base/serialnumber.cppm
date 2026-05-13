/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::SerialNumber into zyppng namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <zypp/base/SerialNumber.h>

export module zyppng:base_serialnumber;

export namespace zyppng {
  using ::zypp::SerialNumber;
  using ::zypp::operator<<;
  using ::zypp::SerialNumberWatcher;
} // namespace zyppng
