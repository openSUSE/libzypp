/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::sat::SolvAttr into zyppng::sat namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <zypp/sat/SolvAttr.h>

export module zyppng:sat_solvattr;

// SolvAttr derives from IdStringType<SolvAttr>. Its free-function operators
// are GMF-attached and not reachable via ADL across the module boundary.
// Re-exporting :idstringtype makes them reachable for any TU that imports :sat_solvattr.
export import :idstringtype;

export namespace zyppng::sat {
  using ::zypp::sat::SolvAttr;
} // namespace zyppng::sat
