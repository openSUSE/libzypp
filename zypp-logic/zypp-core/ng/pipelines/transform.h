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

#include <zypp-core/ng/meta/TypeTraits>
#include <zypp-core/ng/meta/Functional>
#include <algorithm>
#include <zypp-core/ng/pipelines/operators.h>

namespace zyppng {

  namespace detail {

    template <typename ElementType, typename Fn, typename = void>
    struct TransformImpl;

    template <typename ElementType, typename Fn, typename>
    struct TransformImpl {

        using Ret = std::invoke_result_t<Fn, ElementType>;

        template < template< class, class... > class Container,
          typename Transformation = Fn,
          typename ...CArgs >
        static inline Container<Ret> execute( Container<ElementType, CArgs...>&& val, Transformation &&transformation )
        {
          Container<Ret> res;
          std::transform( std::make_move_iterator(val.begin()), std::make_move_iterator(val.end()), std::back_inserter(res), std::forward<Transformation>(transformation) );
          return res;
        }

        template < template< class, class... > class Container,
          typename Transformation = Fn,
          typename ...CArgs >
        static inline Container<Ret> execute(const Container<ElementType, CArgs...>& val, Transformation &&transformation )
        {
          Container<Ret> res;
          std::transform( val.begin(), val.end(), std::back_inserter(res), std::forward<Transformation>(transformation) );
          return res;
        }
    };


  }

template < template< class, class... > class Container,
  typename Msg,
  typename Transformation,
  typename Ret = std::result_of_t<Transformation(Msg)>,
  typename ...CArgs >
auto transform( Container<Msg, CArgs...>&& val, Transformation &&transformation )
{
  return detail::TransformImpl<Msg,Transformation>::execute( std::move(val), std::forward<Transformation>(transformation));
}

template < template< class, class... > class Container,
  typename Msg,
  typename Transformation,
  typename Ret = std::result_of_t<Transformation(Msg)>,
  typename ...CArgs >
auto transform( const Container<Msg, CArgs...>& val, Transformation &&transformation )
{
  return detail::TransformImpl<Msg,Transformation>::execute( val, std::forward<Transformation>(transformation));
}

namespace operators {

  namespace detail {
      template <typename Transformation, typename sfinae = void >
      struct transform_helper {
          Transformation function;

          template< class Container >
          auto operator()( Container&& arg ) {
              return zyppng::transform( std::forward<Container>(arg), function );
          }
      };
  }


  template <typename Transformation>
  auto transform(Transformation&& transformation)
  {
      return detail::transform_helper<Transformation>{
          std::forward<Transformation>(transformation)};
  }
}

}


#ifdef ZYPP_ENABLE_ASYNC
#include <zypp-core/ng/async/pipelines/transform.h>
#endif


#endif
