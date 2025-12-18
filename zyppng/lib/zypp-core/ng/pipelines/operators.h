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
#ifndef ZYPP_NG_ASYNC_PIPELINES_OPERATORS_H_INCLUDED
#define ZYPP_NG_ASYNC_PIPELINES_OPERATORS_H_INCLUDED

#include <zypp-core/ng/async/task.h>

namespace zyppng::operators {

  template< typename In , typename Callback>
  requires ( !Awaitable<std::decay_t<In>> )
  std::invoke_result_t<Callback,In> operator| ( In &&in, Callback &&c )
  {
    return std::forward<Callback>(c)( std::forward<In>(in) );
  }

  // since the following operators are coroutines, we can not use forwarding references
  // we need to enforce copying. Otherwise we risk having a reference stored in the coroutine
  // which might point to a temporary.

  template< Awaitable AwaitableType , typename Callback>
  requires ( std::invocable<Callback, awaitable_res_type_t<std::decay_t<AwaitableType>>> && !Awaitable<std::invoke_result_t<Callback, awaitable_res_type_t<std::decay_t<AwaitableType>>>> )
  Task<std::invoke_result_t<Callback,awaitable_res_type_t<std::decay_t<AwaitableType>>>> operator| ( AwaitableType in, Callback c )
  {
    auto r1 = co_await std::move(in);
    co_return c( std::move(r1) );
  }

  template< Awaitable AwaitableType
            , typename Callback
            , typename Res1 = awaitable_res_type_t<std::decay_t<AwaitableType>>
            , typename Res2Task = std::invoke_result_t<Callback, Res1 &&>
            , typename Res2 = awaitable_res_type_t<Res2Task>
      >
  requires ( std::invocable<Callback, Res1 &&> && Awaitable<Res2Task> )
  Task< Res2 > operator| ( AwaitableType in, Callback c )
  {
    Res1 f = co_await std::move(in);
    Res2 p = co_await c( std::move(f) );
    co_return p;
  }
}

#endif
