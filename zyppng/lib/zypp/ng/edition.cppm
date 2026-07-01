/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::Edition into zyppng namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <ostream>
#include <zypp/Edition.h>

export module zyppng:edition;

// Edition derives from IdStringType<Edition>. Its free-function operators
// (operator==, !=, <, <=, >, >=) are defined in IdStringType.h and are
// GMF-attached — not reachable via ADL across the module boundary.
// Re-exporting :idstringtype makes them reachable for any TU that imports :edition.
export import :idstringtype;

export namespace zyppng {
  using ::zypp::Edition;
  using ::zypp::dumpAsXmlOn;
} // namespace zyppng
