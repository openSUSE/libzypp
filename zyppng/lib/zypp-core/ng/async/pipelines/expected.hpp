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
*
* Based on code by Ivan Čukić (BSD/MIT licensed) from the functional cpp book
*/

#ifndef ZYPP_ZYPPNG_MONADIC_ASYNC_EXPECTED_H
#define ZYPP_ZYPPNG_MONADIC_ASYNC_EXPECTED_H

#include <zypp-core/ng/async/awaitable.h>

namespace zyppng {

  namespace detail {
    template<typename T, typename E, typename Exec>
    struct expected_result_helper<Task<expected<T,E>, Exec>> {
        template <typename ErrType = E>
        static Task<expected<T,E>, Exec> error (  ErrType && error ) {
           co_return expected<T, E>::error( std::forward<ErrType>(error));
        }

        template <typename Res = expected<T,E>>
        static Task<expected<T,E>, Exec> forward ( Res &&exp ) {
          co_return std::forward<Res>(exp);
        }
    };
  }

  template < template< class, class... > class Container,
    typename Msg,
    typename Transformation,
    typename Ret = std::result_of_t<Transformation(Msg)>,
    typename ...CArgs
    >
  requires( is_instance_of_v<Task,Ret> && is_instance_of_v<expected, typename Ret::value_type> )
  Task<expected<Container<typename Ret::value_type::value_type>>> transform_collect( Container<Msg, CArgs...> in, Transformation f )
  {
    Container<typename Ret::value_type::value_type> results;
    for ( auto &v : in ) {
      auto res = co_await f(std::move(v));
      if ( res ) {
        results.push_back( std::move(res.get()) );
      } else {
        co_return expected<Container<typename Ret::value_type::value_type>>::error( res.error() );
      }
    }
    co_return expected<Container<typename Ret::value_type::value_type>>::success( std::move(results) );
  }
}

#endif

