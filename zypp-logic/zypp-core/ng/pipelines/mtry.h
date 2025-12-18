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
#include <zypp-core/base/Exception.h>
#include <zypp-core/ng/pipelines/operators.h>

namespace zyppng {

  namespace detail {

    template <typename Ret, typename F, typename ...Args >
    struct MtryImpl {
        static expected<Ret> execute( F &&f, Args&& ...args )
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
    };
  }

  template < typename F
    , typename ...Args
    , typename Ret = std::invoke_result_t<F, Args...>
    , typename Exp = expected<Ret, std::exception_ptr>
    >
  auto mtry(F &&f, Args&& ...args)
  {
    return detail::MtryImpl<Ret, F, Args...>::execute( std::forward<F>(f), std::forward<Args>(args)... );
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
      return ::zyppng::detail::mtry_helper<Fun> {
        std::forward<Fun>(function)
      };
    }
  }
}

#ifdef ZYPP_ENABLE_ASYNC
#include <zypp-core/ng/async/pipelines/mtry.hpp>
#endif

#endif /* !MTRY_H */
