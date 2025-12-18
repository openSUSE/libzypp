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
#ifndef ZYPP_AWAITABLE_H_INCLUDED
#define ZYPP_AWAITABLE_H_INCLUDED

#include <utility>
namespace zyppng {

  /**
   * \file
   * \brief Provides compile-time definitions for synchronous (non-async) builds.
   *
   * This file contains the definitions used when asynchronous support (coroutines)
   * is disabled at compile time.
   */

  /*!
   * \brief Compile-time flag indicating if asynchronous support is enabled.
   * \ingroup core
   *
   * This constant is set to `true` if libzypp is compiled in asynchronous
   * mode (i.e., with C++20 coroutine support enabled). It will be `false`
   * otherwise.
   *
   * In this build configuration, it is **`false`**.
   *
   * It is intended to be used with `if constexpr` to write code that can
   * be compiled in both synchronous and asynchronous modes.
   *
   * \code
   * if constexpr (ZYPP_IS_ASYNC) {
   * // This block will be compiled out
   * co_await some_task();
   * } else {
   * // This block will be included
   * some_blocking_call();
   * }
   * \endcode
   *
   * \sa MaybeAwaitable
   */
  constexpr bool ZYPP_IS_ASYNC = false;

  /*!
   * \brief A conditional type alias for function return types.
   * \tparam Type The underlying result type of the operation (e.g., `void`, `int`).
   * \ingroup core
   *
   * `MaybeAwaitable` is a helper alias that enables compiling the same
   * function signature for both synchronous (legacy) and asynchronous (NG)
   * builds.
   *
   * - **In Asynchronous Mode (when `ZYPP_IS_ASYNC` is true):**
   * `MaybeAwaitable<Type>` would resolve to `Task<Type>`.
   *
   * - **In Synchronous Mode (when `ZYPP_IS_ASYNC` is false):**
   * This alias resolves to the underlying `Type` itself.
   *
   * This allows a function to be declared as:
   * \code
   * MaybeAwaitable<bool> doSomething();
   * \endcode
   *
   * In an async build, this would be `Task<bool> doSomething()`.
   * In this synchronous build, it is simply `bool doSomething()`,
   * which returns its result directly.
   *
   * \sa ZYPP_IS_ASYNC
   */
  template <class Type>
  using MaybeAwaitable = Type;

  /*!
   * \brief A macro for conditionally awaiting an expression.
   *
   * In an asynchronous build (when `ZYPP_IS_ASYNC` is true), this would
   * expand to the `co_await` keyword.
   *
   * In this synchronous build, it expands to nothing. The expression
   * it precedes is evaluated immediately as a standard function call.
   *
   * \code
   * // In sync build, becomes: int x = getNumber();
   * int x = zypp_co_await getNumber();
   * \endcode
   *
   * \sa ZYPP_IS_ASYNC
   * \sa MaybeAwaitable
   * \sa zypp_co_return
   */
  #define zypp_co_await

  /*!
   * \brief A macro for conditionally returning a value from a function.
   *
   * In an asynchronous build (when `ZYPP_IS_ASYNC` is true), this would
   * expand to the `co_return` keyword.
   *
   * In this synchronous build, it expands to the standard `return` keyword.
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
  #define zypp_co_return return


  template <typename T>
  std::decay_t<T> makeReadyTask( T &&result )
  {
    return std::forward<T>(result);
  }


}
#endif
