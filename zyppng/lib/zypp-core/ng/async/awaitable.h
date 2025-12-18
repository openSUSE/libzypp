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
#ifndef ZYPPNG_AWAITABLE_H_INCLUDED
#define ZYPPNG_AWAITABLE_H_INCLUDED

#include <variant>
#include <zypp-core/ng/meta/type_traits.h>
#include <zypp-core/ng/base/eventdispatcher.h>
#include <coroutine>
#include <concepts>

namespace zyppng {


  struct coro_lazy_execution{};
  struct coro_eager_execution{};

  template <class Result, class Execution = coro_lazy_execution>
  class Task;

  template<typename T, typename E>
  class expected;


  /*!
   * Check if one of the variadic types is the same as T
   */
  template<typename T, typename ... U>
  concept either = (std::same_as<T, U> || ...);


  template <typename T>
  concept AwaiterInterface = requires ( T t, std::coroutine_handle<> hdl )
  {
    { t.await_ready() } -> std::convertible_to<bool>;
    { t.await_suspend(hdl) };
    { t.await_resume()};
  };

  template <typename T>
  concept has_co_await_member_operator = requires ( T &&t )
  {
    { std::forward<T>(t).operator co_await() };
  };

  template <typename T>
  concept has_co_await_global_operator = requires ( T &&t )
  {
    { operator co_await(std::forward<T>(t)) };
  };

  template <typename T>
  concept has_co_await_operator =  has_co_await_global_operator<T> || has_co_await_member_operator<T>;

  template <typename T>
  concept is_co_awaitable = AwaiterInterface<T> || has_co_await_global_operator<T> || has_co_await_member_operator<T>;

  template <typename T>
  concept Awaitable =
      !std::is_copy_constructible_v<T> && // single ownership! we do not support copying awaitables
      is_co_awaitable<T>;


  namespace detail {

    template <typename T>
    struct awaitable_res_type_impl;

    template <typename T>
    requires ( AwaiterInterface<T> )
    struct awaitable_res_type_impl<T> {
        using type = std::decay_t< decltype( std::declval<T>().await_resume() ) >;
    };

    template <typename T>
    requires (has_co_await_global_operator<T>)
    struct awaitable_res_type_impl<T> {
        using type = typename awaitable_res_type_impl<decltype( operator co_await( std::declval<T>() ) )>::type;
    };

    template <typename T>
    requires (has_co_await_member_operator<T>)
    struct awaitable_res_type_impl<T> {
        using type = typename awaitable_res_type_impl<decltype( ( std::declval<T>().operator co_await() ) )>::type;
    };
  }

  /*!
   * \brief calculates the value_type of a type fulfilling the Awaitable concept.
   *
   * This helper is required because the Awaitable concept also supports wrapping
   * a type that implements the Awaitable interface inside a pointer type like:
   *
   * \code
   * std::unique_ptr<Task<int>>.
   * \endcode
   */
  template <class T>
  using awaitable_res_type_t = typename detail::awaitable_res_type_impl<std::remove_cvref_t<T>>::type;

}


namespace zyppng {
  /*!
   * \brief Compile-time flag indicating if asynchronous support is enabled.
   *
   * This constant is set to `true` if libzypp is compiled in asynchronous
   * mode (i.e., with C++20 coroutine support enabled). It will be `false`
   * otherwise.
   *
   * It is intended to be used with `if constexpr` to write code that can
   * be compiled in both synchronous and asynchronous modes.
   *
   * \code
   * if constexpr (ZYPP_IS_ASYNC) {
   * // Await an async operation
   * co_await some_task();
   * } else {
   * // Call a blocking function
   * some_blocking_call();
   * }
   * \endcode
   *
   * \sa MaybeAwaitable
   * \sa See `awaitable.h` in zypp legacy for the corresponding non-async variable.
   */
  constexpr bool ZYPP_IS_ASYNC = true;

  /*!
   * \brief A conditional type alias for function return types.
   * \tparam Type The underlying result type of the operation (e.g., `void`, `int`).
   *
   * `MaybeAwaitable` is a helper alias that enables compiling the same
   * function signature for both synchronous (legacy) and asynchronous (NG)
   * builds.
   *
   * - **In Asynchronous Mode (when `ZYPP_IS_ASYNC` is true):**
   * `MaybeAwaitable<Type>` resolves to `Task<Type>`.
   *
   * - **In Synchronous Mode (when `ZYPP_IS_ASYNC` is false):**
   * This alias would resolve to the underlying `Type` itself.
   *
   * This allows a function to be declared as:
   * \code
   * MaybeAwaitable<bool> doSomething();
   * \endcode
   *
   * In an async build, this is `Task<bool> doSomething()`, which can be implemented
   * as a coroutine. In a sync build, this is `bool doSomething()`, which
   * returns its result directly.
   *
   * \sa ZYPP_IS_ASYNC
   * \sa Task
   */
  template <class Type, class Execution = coro_lazy_execution >
  using MaybeAwaitable = Task<Type, Execution>;


  /*!
   * \brief A macro for conditionally awaiting an expression.
   *
   * In an asynchronous build (when `ZYPP_IS_ASYNC` is true), this
   * expands to the `co_await` keyword.
   *
   * In a synchronous build, it would expand to nothing
   * ( see the corresponding awaitable.h in zypp legacy ).
   *
   * \code
   * // In async build, becomes: int x = co_await getNumberAsync();
   * int x = zypp_co_await getNumberAsync();
   * \endcode
   *
   * \sa ZYPP_IS_ASYNC
   * \sa MaybeAwaitable
   * \sa zypp_co_return
   */
  #define zypp_co_await co_await

  /*!
   * \brief A macro for conditionally returning a value from a function.
   *
   * In an asynchronous build (when `ZYPP_IS_ASYNC` is true), this
   * expands to the `co_return` keyword.
   *
   * In a synchronous build, it would expand to the standard `return` keyword.
   * ( see the corresponding awaitable.h in zypp legacy ).
   *
   * \code
   * MaybeAwaitable<int> myFunction() {
   * // In sync build: return 42;
   * // In async build: co_return 42;
   * zypp_co_return 42;
   * }
   * \endcode
   *
   * \sa ZYPP_IS_ASYNC
   * \sa MaybeAwaitable
   * \sa zypp_co_await
   */
  #define zypp_co_return co_return
}

#endif
