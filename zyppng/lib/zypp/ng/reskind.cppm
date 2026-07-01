/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::ResKind into zyppng namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <zypp/ResKind.h>

export module zyppng:reskind;
export import :idstringtype;

export namespace zyppng {
  using ::zypp::ResKind;
  using ::zypp::dumpAsXmlOn;
} // namespace zyppng
