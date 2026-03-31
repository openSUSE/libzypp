/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-core/ng/base/ranges.h
 *
 * Namespace routing for C++20 ranges and C++23 ranges::to<T>() backport.
 *
 * Consumers must always include this header and use zyppng::ranges::
 * rather than nano:: or std::ranges:: directly. This ensures a
 * zero-touch migration to std::ranges once the compiler baseline
 * supports it:
 *
 *   #include <zypp-core/ng/base/ranges.h>
 *
 *   auto result = myRange
 *     | zyppng::ranges::views::filter( pred )
 *     | zyppng::ranges::views::transform( xform )
 *     | zyppng::ranges::to<std::vector<Foo>>();
 *
 * Do NOT include this header in legacy (non-ng/) code.
 * Do NOT include <nanorange/nanorange.hpp> or <ranges> directly.
 */
#ifndef ZYPPNG_BASE_RANGES_H
#define ZYPPNG_BASE_RANGES_H

#include <iterator>
#include <type_traits>
#include <zypp-core/ng/meta/type_traits.h>

// ---------------------------------------------------------------------------
// 1. Pull the polyfill backend into an internal namespace.
//    Consumers never touch _detail::polyfill directly.
// ---------------------------------------------------------------------------

#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 201911L && !defined(ZYPPNG_FORCE_NANORANGE)
#  include <ranges>
   namespace zyppng::_detail { namespace polyfill = std::ranges; }
#else
#  include <nanorange/nanorange.hpp>
   namespace zyppng::_detail { namespace polyfill = nano::ranges; }
#endif

// ---------------------------------------------------------------------------
// 2. zyppng::ranges — the single public interface.
//
//    We define zyppng::ranges as a true namespace (not an alias) so that
//    we can extend it with our own additions (e.g. ranges::to) while still
//    exposing the full polyfill API via using-declarations.
// ---------------------------------------------------------------------------

namespace zyppng::ranges {

    // Expose the full polyfill API: all algorithms, views, concepts.
    using namespace ::zyppng::_detail::polyfill;

    // views sub-namespace: zyppng::ranges::views::filter etc.
    namespace views = ::zyppng::_detail::polyfill::views;

} // namespace zyppng::ranges

// ---------------------------------------------------------------------------
// 3. ranges::to<Container>() — C++23 backport for C++17
//
//    std::ranges::to is C++23 and absent from both std::ranges (C++20) and
//    nanorange. We provide our own implementation here so that all range
//    pipelines can terminate with a uniform | zyppng::ranges::to<T>() call
//    regardless of compiler baseline.
// ---------------------------------------------------------------------------

namespace zyppng::ranges {

namespace detail {

    // Detector ops — uses the detector idiom from type_traits.h
    template<typename T>
    using reserve_op = decltype( std::declval<T&>().reserve( std::declval<std::size_t>() ) );

    template<typename T>
    using size_op = decltype( std::declval<const T&>().size() );

    // Reserve capacity upfront if both Container and Range support it.
    template<typename Container, typename Range>
    void maybe_reserve( Container & c, const Range & r )
    {
        if constexpr ( std::is_detected_v<reserve_op, Container>
                    && std::is_detected_v<size_op,    Range> )
            c.reserve( r.size() );
    }

} // namespace detail

    /**
     * Convert any range into a Container.
     *
     * - Reserves capacity upfront if Container supports .reserve()
     *   and Range supports .size().
     * - Moves elements if Range&& is an rvalue reference, copies otherwise.
     *
     * Usage:
     *   auto v = myRange | zyppng::ranges::to<std::vector<int>>();
     *   auto s = zyppng::ranges::to<std::set<Foo>>( filteredRange );
     */
    template<typename Container, typename Range>
    Container to( Range && range )
    {
        Container result;
        detail::maybe_reserve( result, range );

        // std::is_rvalue_reference<Range&&> recovers the value category via
        // reference collapsing: lvalue -> Range = T& -> Range&& = T& -> false
        //                        rvalue -> Range = T  -> Range&& = T&& -> true
        if constexpr ( std::is_rvalue_reference<Range&&>::value ) {
            std::move( std::begin(range), std::end(range),
                       std::inserter(result, std::end(result)) );
        } else {
            std::copy( std::begin(range), std::end(range),
                       std::inserter(result, std::end(result)) );
        }
        return result;
    }

    // Pipe adaptor: range | zyppng::ranges::to<Container>()
    template<typename Container>
    struct to_fn {
        template<typename Range>
        friend Container operator|( Range && range, to_fn )
        {
            return zyppng::ranges::to<Container>( std::forward<Range>(range) );
        }
    };

    template<typename Container>
    to_fn<Container> to() { return {}; }

} // namespace zyppng::ranges

#endif // ZYPPNG_BASE_RANGES_H
