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

#ifndef ZYPP_ZYPPNG_ASYNC_MONADIC_MTRY_H
#define ZYPP_ZYPPNG_ASYNC_MONADIC_MTRY_H

#include <zypp-core/ng/async/awaitable.h>

namespace zyppng {

  namespace detail {

    template <typename Ret, typename Exec, typename F, typename ...Args >
    struct MtryImpl<Task<Ret, Exec>, F, Args...> {
        static Task<expected<Ret>> execute( F &&f, Args&& ...args )
        {
          try {
            if constexpr ( std::is_same_v<void, Ret> ) {
              co_await std::invoke(std::forward<F>(f), std::forward<Args>(args)... );
              co_return expected<Ret, std::exception_ptr>::success();
            } else {
              co_return expected<Ret, std::exception_ptr>::success( co_await std::invoke(std::forward<F>(f), std::forward<Args>(args)... ) );
            }
          } catch (...) {
            co_return expected<Ret, std::exception_ptr>::error(ZYPP_FWD_CURRENT_EXCPT());
          }
        }
    };

  }
}

#endif /* !MTRY_H */
