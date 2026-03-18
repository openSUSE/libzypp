/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-core/ng/base/precondition.h
 *
 * Always-on precondition checking for NG code.
 *
 * This header is intentionally NOT installed as public API.
 * It is private to the zyppng implementation layer.
 */
#ifndef ZYPPNG_BASE_PRECONDITION_H
#define ZYPPNG_BASE_PRECONDITION_H

#include <zypp-core/base/LogTools.h>

/// In C++20 and later the violation branch is marked [[unlikely]], giving the
/// optimiser a free hint that it is never expected to be taken.
/// In C++17 the attribute is omitted — the macro is otherwise identical.
#if __cplusplus >= 202002L
#  define ZYPP_DETAIL_UNLIKELY [[unlikely]]
#else
#  define ZYPP_DETAIL_UNLIKELY
#endif

/** Always-on precondition check — fires in debug AND release builds.
 *
 * On violation: logs file / line / function / expression to the INT
 * (internal error) log channel, then calls std::terminate().
 * The failure is not recoverable and must never be caught.
 *
 * An optional human-readable message may follow the expression:
 * \code
 *   ZYPP_PRECONDITION( ptr != nullptr );
 *   ZYPP_PRECONDITION( id != noId, "pool() called on a null handle" );
 * \endcode
 */
#define ZYPP_PRECONDITION( EXPR, ... ) \
  do { \
    if ( !( EXPR ) ) ZYPP_DETAIL_UNLIKELY \
      ::zyppng::detail::preconditionViolated( \
          __FILE__, __LINE__, __FUNCTION__, #EXPR, "" __VA_ARGS__ ); \
  } while ( false )

namespace zyppng::detail {

  [[noreturn]] inline void preconditionViolated(
      const char * file,
      int          line,
      const char * func,
      const char * expr,
      const char * msg )
  {
    INT << file << "(" << func << "):" << line
        << ": PRECONDITION VIOLATED: (" << expr << ")"
        << ( msg && *msg ? " — " : "" )
        << ( msg && *msg ?  msg  : "" )
        << std::endl;
    std::terminate();
  }

} // namespace zyppng::detail

#endif // ZYPPNG_BASE_PRECONDITION_H
