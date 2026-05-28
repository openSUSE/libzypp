/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
// zyppng:sat_poolmember -- pure C++20 template, no legacy includes required.
// PoolMember<Derived> is a CRTP base that routes pool() calls through
// explicit specialisations of detail::poolFromType<T>.  Each sat type
// that inherits PoolMember<T> must provide its own specialisation
// (declared in its .cppm, defined in its .cc).  A missing specialisation
// produces a linker error, not a silent fallback.
module;
// GMF required by GCC's P1689R5 dyndep scanner to correctly register
// the 'import :sat_poolconstants' dependency below.
#include <zypp-core/Globals.h>

export module zyppng:sat_poolmember;

import :sat_poolconstants;  // for zyppng::sat::Pool forward

// Pool's full definition (exported) lives in zyppng:sat_pool.  Forward
// declared here with matching export linkage so other partitions that
// import :sat_poolmember see a linkage-consistent declaration.
export namespace zyppng::sat { class Pool; }

namespace zyppng::sat::detail {
  // Primary templates — intentionally left undefined.
  // Each sat type that inherits PoolMember<T> provides its own specialisation
  // declared in its .cppm, defined in its .cc.
  template <typename T> ::zyppng::sat::Pool &       poolFromType( T & );
  template <typename T> const ::zyppng::sat::Pool & poolFromType( const T & );
} // namespace zyppng::sat::detail

export namespace zyppng::sat {

  template <typename Derived>
  class PoolMember
  {
  public:
    Pool & pool()
    { return detail::poolFromType( static_cast<Derived &>( *this ) ); }

    const Pool & pool() const
    { return detail::poolFromType( static_cast<const Derived &>( *this ) ); }
  };

} // namespace zyppng::sat
