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

#ifndef ZYPP_ZYPPNG_MONADIC_MTRY_H
#define ZYPP_ZYPPNG_MONADIC_MTRY_H

#include "expected.h"

namespace zyppng {

  template < typename F
    , typename ...Args
    , typename Ret = std::invoke_result_t<F, Args...>
    , typename Exp = expected<Ret, std::exception_ptr>
    >
  Exp mtry(F &&f, Args&& ...args)
  {
    try {
      if constexpr ( std::is_same_v<void, Ret> ) {
        std::invoke(std::forward<F>(f), std::forward<Args>(args)... );
        return expected<Ret, std::exception_ptr>::success();
      } else {
        return expected<Ret, std::exception_ptr>::success(std::invoke(std::forward<F>(f), std::forward<Args>(args)... ));
      }
    } catch (...) {
      return expected<Ret, std::exception_ptr>::error(ZYPP_FWD_CURRENT_EXCPT());
    }
  }


  namespace detail
  {
    template <typename Callback>
    struct mtry_helper {
      Callback function;

      template < typename ...Args >
      auto operator()( Args&& ...args ){
        return mtry( function, std::forward<Args>(args)... );
      }
    };
  }

  namespace operators {
    template <typename Fun>
    auto mtry ( Fun && function ) {
      return detail::mtry_helper<Fun> {
        std::forward<Fun>(function)
      };
    }
  }


}

#endif /* !MTRY_H */
