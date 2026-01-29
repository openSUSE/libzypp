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
#ifndef ZYPPNG_MONADIC_WAIT_H_INCLUDED
#define ZYPPNG_MONADIC_WAIT_H_INCLUDED

#include <zypp-core/ng/pipelines/operators.h>

#ifndef ZYPP_ENABLE_ASYNC
namespace zyppng {
  namespace operators {

    namespace detail {
      struct JoinHelper {
          template <typename T>
          auto operator() ( T &&ops ) {
            return std::forward<T>(ops);
          }
      };
    }
    /*!
     *  No-Op operation, just here for compatibility when compiling in sync mode
     */
    inline auto join ( ) {
      return detail::JoinHelper();
    }
  }
}
#endif

#ifdef ZYPP_ENABLE_ASYNC
#include <zypp-core/ng/async/pipelines/wait.hpp>
#endif

#endif
