/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-BRIDGE: wraps zypp::ResTraits into zyppng namespace.
// Replace with native C++20 implementation when zypp/ legacy is severed.
module;
#include <zypp/ResTraits.h>

export module zyppng:restraits;

export namespace zyppng::traits {
  using ::zypp::traits::isPseudoInstalled;
} // namespace zyppng::traits

export namespace zyppng {
  using ::zypp::Resolvable;
  using ::zypp::ResObject;
  using ::zypp::Package;
  using ::zypp::SrcPackage;
  using ::zypp::Pattern;
  using ::zypp::Product;
  using ::zypp::Patch;
  using ::zypp::Application;
  using ::zypp::PoolItem;
  using ::zypp::ResTraits;
  using ::zypp::resKind;
  using ::zypp::isKind;
} // namespace zyppng
