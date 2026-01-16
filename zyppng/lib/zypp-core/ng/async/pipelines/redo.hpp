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
#ifndef ZYPPNG_MONADIC_ASYNC_REDO_H_INCLUDED
#define ZYPPNG_MONADIC_ASYNC_REDO_H_INCLUDED

namespace zyppng {

  template <typename Result>
  class Task;

  namespace detail {
    template< typename Fn, typename Pred, typename Arg >
    struct RedoWhileImpl< Fn, Pred, Arg, std::enable_if_t<is_instance_of_v<Task, std::invoke_result_t<Fn,Arg>>> >
    {
        using Ret = std::invoke_result_t<Fn,Arg>;
        template <typename T = Arg>
        static Task<typename Ret::value_type> execute( Fn task, Pred pred, T &&arg  ) {
          Arg store = std::forward<T>(arg);
          do {
            auto res = co_await task ( Arg(store) );

            if constexpr ( is_instance_of_v<Task, std::invoke_result_t<Pred, typename Ret::value_type> > ) {
              if ( !co_await(pred( res ) ) )
                co_return std::move(res);
            } else {
              if ( !pred( res ) )
                co_return std::move(res);
            }
          } while( true );
        }
    };
  }
}

#endif
