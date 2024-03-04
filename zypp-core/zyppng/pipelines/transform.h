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


#ifndef ZYPP_ZYPPNG_MONADIC_TRANSFORM_H
#define ZYPP_ZYPPNG_MONADIC_TRANSFORM_H

#include <zypp-core/zyppng/meta/TypeTraits>
#include <zypp-core/zyppng/meta/Functional>
#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp-core/zyppng/pipelines/wait.h>
#include <algorithm>

namespace zyppng {

template < template< class, class... > class Container,
  typename Msg,
  typename Transformation,
  typename Ret = std::result_of_t<Transformation(Msg)>,
  typename ...CArgs >
Container<Ret> transform( Container<Msg, CArgs...>&& val, Transformation &&transformation )
{
  Container<Ret> res;
  std::transform( std::make_move_iterator(val.begin()), std::make_move_iterator(val.end()), std::back_inserter(res), std::forward<Transformation>(transformation) );
  return res;
}

template < template< class, class... > class Container,
  typename Msg,
  typename Transformation,
  typename Ret = std::result_of_t<Transformation(Msg)>,
  typename ...CArgs >
Container<Ret> transform( const Container<Msg, CArgs...>& val, Transformation &&transformation )
{
  Container<Ret> res;
  std::transform( val.begin(), val.end(), std::back_inserter(res), std::forward<Transformation>(transformation) );
  return res;
}

namespace detail {
    template <typename Transformation >
    struct transform_helper {
        Transformation function;

        template< class Container >
        auto operator()( Container&& arg ) {
          if constexpr ( detail::is_sync_monad_cb_with_async_res_v<Transformation, typename Container::value_type> ) {
            using namespace zyppng::operators;
            return zyppng::transform( std::forward<Container>(arg), function ) | zyppng::waitFor();
          } else {
            return zyppng::transform( std::forward<Container>(arg), function );
          }
        }
    };
}

namespace operators {

    template <typename Transformation>
    auto transform(Transformation&& transformation)
    {
        return detail::transform_helper<Transformation>{
            std::forward<Transformation>(transformation)};
    }
}

}

#endif
