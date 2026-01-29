/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*/


#ifndef ZYPP_ZYPPNG_MONADIC_ASYNC_TRANSFORM_H
#define ZYPP_ZYPPNG_MONADIC_ASYNC_TRANSFORM_H

#include <zypp-core/ng/async/awaitable.h>
#include <zypp-core/ng/meta/TypeTraits>
#include <zypp-core/ng/meta/Functional>
#include <algorithm>

namespace zyppng {

  namespace detail {

    template <typename ElementType, typename Fn, typename>
    struct TransformImpl;

    template <typename ElementType, typename Fn>
    requires( Awaitable< std::invoke_result_t<Fn, ElementType> > )
    struct TransformImpl< ElementType, Fn, void> {

        using Ret = awaitable_res_type_t<std::invoke_result_t<Fn,ElementType>>;

        template < template< class, class... > class Container,
          typename Transformation = Fn,
          typename ...CArgs >
        static inline Task<Container<Ret>> execute( Container<ElementType, CArgs...>&& val, Transformation &&transformation )
        {
          Container<Ret> res;
          std::back_insert_iterator<Container<Ret>> insert_iter(res);
          for ( auto &elem : val ) {
            insert_iter = co_await( transformation(std::move(elem)) );
          }
          co_return res;
        }

        template < template< class, class... > class Container,
          typename Transformation = Fn,
          typename ...CArgs >
        static inline Container<Ret> execute(const Container<ElementType, CArgs...>& val, Transformation &&transformation )
        {
          Container<Ret> res;
          std::back_insert_iterator<Container<Ret>> insert_iter(res);
          for ( const auto &elem : val ) {
            insert_iter = co_await( transformation(elem) );
          }
          co_return res;
        }
    };


  }

}

#endif
