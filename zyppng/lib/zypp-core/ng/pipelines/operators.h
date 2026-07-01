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

  // The sync overload's return type MUST be deduced (`auto`).
  //
  // Clang (≤19) substitutes an explicit return type like
  // `std::invoke_result_t<Callback, In>` independently of the constraint
  // check during overload resolution. For an Awaitable LHS this poisons
  // the user-supplied callback (typically a lambda) by instantiating it
  // with the wrong argument type, producing baffling errors deep in user
  // code instead of cleanly rejecting this overload. GCC defers correctly.
  //
  // The async overloads below are not affected: their return types embed
  // `awaitable_res_type_t<...>`, an undefined-for-non-Awaitable trait that
  // hard-fails substitution before reaching `invoke_result_t`.
  //
  // `NotAwaitable` is used here for readability only — it does NOT fix the
  // bug on its own; only `auto` does.
  template< NotAwaitable In , typename Callback>
  requires ( std::invocable<Callback, In> )
  auto operator| ( In &&in, Callback &&c )
  {
    static_assert( NotAwaitable<In>,
      "Sync operator| selected for an Awaitable LHS — see header comment." );
    return std::forward<Callback>(c)( std::forward<In>(in) );
  }

  // since the following operators are coroutines, we can not use forwarding references
  // we need to enforce copying. Otherwise we risk having a reference stored in the coroutine
  // which might point to a temporary.

  template< Awaitable AwaitableType , typename Callback>
  requires ( std::invocable<Callback, awaitable_res_type_t<std::decay_t<AwaitableType>>> && !Awaitable<std::invoke_result_t<Callback, awaitable_res_type_t<std::decay_t<AwaitableType>>>> )
  Task<std::invoke_result_t<Callback,awaitable_res_type_t<std::decay_t<AwaitableType>>>> operator| ( AwaitableType in, Callback c )
  {
    static_assert( Awaitable<std::decay_t<AwaitableType>>,
      "async operator| (sync result) selected for non-Awaitable LHS." );
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
    static_assert( Awaitable<std::decay_t<AwaitableType>>,
      "async operator| (async result) selected for non-Awaitable LHS." );
    static_assert( Awaitable<Res2Task>,
      "async operator| (async result) selected but callback result is not Awaitable." );
    Res1 f = co_await std::move(in);
    Res2 p = co_await c( std::move(f) );
    co_return p;
  }
}

#endif
