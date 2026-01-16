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
*/
#ifndef ZYPPNG_ASYNC_PIPELINES_ALGORITHM_H_INCLUDED
#define ZYPPNG_ASYNC_PIPELINES_ALGORITHM_H_INCLUDED

#include <zypp-core/ng/async/awaitable.h>

namespace zyppng {

  namespace detail {
    template < class Container, class Transformation, class Predicate, class DefaultType >
    struct FirstOfImpl<Container, Transformation, Predicate, DefaultType, std::enable_if_t< is_instance_of_v< Task, std::invoke_result_t<Transformation, typename Container::value_type> > > >
    {
        using InputType  = typename Container::value_type;
        using OutputType = std::invoke_result_t<Transformation, InputType>;

        static_assert( std::is_invocable_v<Transformation, InputType>, "Transformation function must take the container value type as input " );
        static_assert( std::is_same_v<OutputType, DefaultType>, "Default type and transformation result type must match" );

        static Task<OutputType> execute ( Container container, Transformation transFunc, DefaultType defaultVal, Predicate predicate) {
          for ( auto &in : container ) {
            OutputType res = co_await std::invoke( transFunc, std::move(in) );
            if ( _predicate(res) ) {
              co_return res;
            }
          }
          co_return defaultVal;
        }
    };
  }
}
#endif
