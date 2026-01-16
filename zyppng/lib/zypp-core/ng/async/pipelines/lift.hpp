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
#ifndef ZYPPNG_MONADIC_ASYNC_LIFT_H_INCLUDED
#define ZYPPNG_MONADIC_ASYNC_LIFT_H_INCLUDED

namespace zyppng {

  template <typename Result> class Task;

  namespace detail {
    template <typename LiftedFun, typename T1, typename T2 >
    requires( is_instance_of_v<Task, std::invoke_result_t< LiftedFun, T1> > )
    struct LiftImpl<LiftedFun, T1, T2 > >  {
      using Ret = typename std::invoke_result_t< LiftedFun, T1>::value_type;

      static Task<std::pair<Ret, T2>> execute( LiftedFun fun, std::pair<T1, T2> &&data ) {
        auto r = co_await std::invoke( fun, std::move(data.first) );
        co_return std::make_pair( std::move(r), std::move(data.second) );
      }
    };
  }

}

#endif
