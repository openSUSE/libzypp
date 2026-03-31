/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-core/ng/base/span.h
 *
 * Namespace routing for C++20 std::span.
 *
 * Consumers must always include this header and use zyppng::span
 * rather than tcb::span or std::span directly. This ensures a
 * zero-touch migration to std::span once the compiler baseline
 * supports it:
 *
 *   #include <zypp-core/ng/base/span.h>
 *
 *   void process( zyppng::span<const std::byte> data );
 *
 * Do NOT include this header in legacy (non-ng/) code.
 * Do NOT include <tcb/span.hpp> or <span> directly.
 */
#ifndef ZYPPNG_BASE_SPAN_H
#define ZYPPNG_BASE_SPAN_H

#include <cstddef>

#if defined(__cpp_lib_span) && __cpp_lib_span >= 202002L && !defined(ZYPPNG_FORCE_TCB_SPAN)
#  include <span>
   namespace zyppng {
       using std::span;
       using std::dynamic_extent;
   }
#else
#  include <tcb/span.hpp>
   namespace zyppng {
       using tcb::span;
       using tcb::dynamic_extent;
   }
#endif

#endif // ZYPPNG_BASE_SPAN_H
