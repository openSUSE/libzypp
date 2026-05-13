/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::LanguageCode into zyppng namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <zypp/LanguageCode.h>

export module zyppng:languagecode;

export namespace zyppng {
  using ::zypp::LanguageCode;
} // namespace zyppng
