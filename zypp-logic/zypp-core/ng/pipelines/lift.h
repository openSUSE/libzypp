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
#ifndef ZYPPNG_MONADIC_LIFT_H_INCLUDED
#define ZYPPNG_MONADIC_LIFT_H_INCLUDED

#include <utility>

#include <zypp-core/ng/meta/Functional>
#include <zypp-core/ng/async/task.h>
#include <zypp-core/ng/pipelines/operators.h>

namespace zyppng {

  namespace detail {

  template <typename LiftedFun, typename T1, typename T2, typename = void >
  struct LiftImpl {
    static auto execute( LiftedFun fun, std::pair<T1, T2> &&data ) {
      return std::make_pair( std::invoke( fun, std::move(data.first) ), std::move(data.second) );
    }
  };

  template <typename LiftedFun >
  struct lifter {

    lifter( LiftedFun &&fun ) : _fun(std::move(fun)) {}
    lifter( lifter && ) = default;
    ~lifter(){}

    template< typename T1
      , typename T2
      >
    auto operator()( std::pair<T1, T2> &&data ) {
      return LiftImpl<LiftedFun, T1, T2>::execute( std::move(_fun), std::move(data) );
    }
  private:
    LiftedFun _fun;
  };
  }

  template< typename Fun >
  auto lift ( Fun && func ) {
    return detail::lifter<Fun>( std::forward<Fun>(func) );
  }

}


#ifdef ZYPP_ENABLE_ASYNC
#include <zypp-core/ng/async/pipelines/lift.hpp>
#endif

#endif
