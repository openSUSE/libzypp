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
#ifndef ZYPPNG_MONADIC_REDO_H_INCLUDED
#define ZYPPNG_MONADIC_REDO_H_INCLUDED

#include <zypp-core/ng/meta/FunctionTraits>
#include <zypp-core/ng/meta/TypeTraits>
#include <zypp-core/ng/meta/Functional>
#include <zypp-core/ng/pipelines/operators.h>

namespace zyppng {

  namespace detail {

  template< typename Task, typename Pred, typename Arg, typename = void >
  struct RedoWhileImpl
  {
      template <typename T = Arg>
      static auto execute( Task task, Pred pred, T &&arg  ) {
        Arg store = std::forward<T>(arg);
        do {
          auto res = task ( Arg(store) );
          if ( !pred( res ) )
            return std::move(res);
        } while( true );
      }
  };

  template< typename Task, typename Pred >
  struct RedoWhileHelper
  {
      template <typename T, typename P>
      RedoWhileHelper( T &&t, P &&p ) :
        _task( std::forward<T>(t) )
        , _pred( std::forward<P>(p) ) {}

      template <typename Arg>
      auto operator()( Arg &&arg ) {
        return RedoWhileImpl<Task,Pred, Arg>::execute( std::move(_task), std::move(_pred), std::forward<Arg>(arg) );
      }

      template <typename T, typename P>
      static auto create ( T &&t, P &&p ) {
        return RedoWhileImpl( std::forward<T>(t), std::forward<P>(p));
      }

    private:
      Task _task;
      Pred _pred;
    };
  }

  template <typename Task, typename Pred>
  auto redo_while ( Task &&todo, Pred &&until ) {
    return detail::RedoWhileHelper<Task,Pred>::create( std::forward<Task>(todo), std::forward<Pred>(until) );
  }

}

#ifdef ZYPP_ENABLE_ASYNC
#include <zypp-core/ng/async/pipelines/redo.hpp>
#endif

#endif
