/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::ResolverNamespace into zyppng namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <zypp/ResolverNamespace.h>

export module zyppng:resolvernamespace;

export namespace zyppng {
  using ::zypp::ResolverNamespace;
  using ::zypp::NoResolverNamespaces;
  using ::zypp::AllResolverNamespaces;
  using ::zypp::asIdString;
  using ::zypp::asString;
  using ::zypp::operator<<;
} // namespace zyppng
