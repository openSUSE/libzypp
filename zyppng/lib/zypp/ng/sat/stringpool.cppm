/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// LEGACY-REQUIRED: StringPool is still consumed by zypp/ legacy code.
// The source remains in zypp-logic/zypp/ng/sat/stringpool.{h,cc}.
// Do not remove until the zypp/ dependency on StringPool is severed.
module;
#include <zypp/ng/sat/solvincludes.h>
#include <zypp/sat/detail/PoolDefines.h>
#include <zypp/ng/sat/stringpool.h>

export module zyppng:sat_stringpool;

export namespace zyppng::sat {
  using ::zyppng::sat::StringPool;
} // namespace zyppng::sat
