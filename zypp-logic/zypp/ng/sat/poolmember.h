/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef POOLMEMBER_H_ZYPPNG_WRAPPER_H
#define POOLMEMBER_H_ZYPPNG_WRAPPER_H

namespace zyppng::sat {

  class Pool;

  namespace detail {
    // Primary templates — intentionally left undefined.
    // Each type that inherits PoolMember<T> must provide an explicit
    // specialization (declared in its .h, defined in its .cc).
    // A missing specialization produces a linker error, not a silent
    // fallback to the global singleton.
    template <typename T> Pool &       poolFromType( T & );
    template <typename T> const Pool & poolFromType( const T & );
  }

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
#endif // POOLMEMBER_H_ZYPPNG_WRAPPER_H
