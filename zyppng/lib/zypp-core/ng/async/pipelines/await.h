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
#ifndef ZYPPNG_MONADIC_AWAIT_H_INCLUDED
#define ZYPPNG_MONADIC_AWAIT_H_INCLUDED


#include <zypp-core/ng/async/task.h>
#include <zypp-core/ng/base/Signals>

namespace zyppng {

  namespace detail
  {
    template<typename T>
    struct signal_arg_helper_impl {
      using type = T;
    };

    template <typename T>
    struct signal_arg_helper_impl<T &> {
      using type = std::reference_wrapper<T>;
    };

    template <typename T>
    struct signal_arg_helper_impl<const T &> {
      using type = std::reference_wrapper<const T>;
    };

    template <typename T>
    using signal_arg_helper_t = typename signal_arg_helper_impl< std::remove_volatile_t<T> >::type;

    template <class... Args>
    struct signal_awaiter_ret_helper;

    template <>
    struct signal_awaiter_ret_helper<>
    {
        using ret_type = void;
        bool val = false; //just a flag to signal that the sl
    };

    template < class... Args >
    struct signal_awaiter_ret_helper
    {
        using ret_type = std::tuple< signal_arg_helper_t<Args>...> ;
        std::optional<ret_type> val;
    };

    template <class SigRet, class... SigArgs>
    struct SignalAwaiter;


    template <class SigRet, class... SigArgs>
    struct SignalAwaiter<SigRet(SigArgs...)>
    {
      using AwaitRet = typename signal_awaiter_ret_helper<SigArgs...>::ret_type;

      SignalAwaiter(SignalProxy<SigRet(SigArgs...)> &&proxy)
        : _conn( proxy.connect( sigc::mem_fun (*this, &SignalAwaiter::template sigHandler<SigArgs...> )) ) { }

      SignalAwaiter(const SignalAwaiter &) = delete;
      SignalAwaiter(SignalAwaiter &&) = delete;
      SignalAwaiter &operator=(const SignalAwaiter &) = delete;
      SignalAwaiter &operator=(SignalAwaiter &&) = delete;

      ~SignalAwaiter() {
        _conn.disconnect();
      }

      bool await_ready() const noexcept
      {
        if constexpr ( !std::is_same_v<void,AwaitRet> ) {
          return _returnContainer.val.has_value();
        } else {
          return _returnContainer.val;
        }
      }

      void await_suspend(std::coroutine_handle<> cont)
      {
        if ( await_ready() ) {
          cont.resume();
          return;
        }
        _coro = std::move(cont);
      }

      AwaitRet await_resume() {
        if constexpr ( !std::is_same_v<void,AwaitRet> ) {
            return (*_returnContainer.val);
        }
      }

      private:
        template <typename... Args>
        SigRet sigHandler ( Args... args ) {

          _conn.disconnect(); // receive just once

          // store arguments, or set the flag that we received the signal
          if constexpr ( sizeof...( args) != 0 ) {
            _returnContainer.val = std::tuple<Args...> { std::forward<Args>(args)... };
          } else {
            _returnContainer.val = true;
          }

          if ( _coro ) {
            // invoke right away when entering the event loop again
            zyppng::EventDispatcher::invokeAfter( [c = std::move(_coro)](){
              c.resume();
              return false;
            } , 0 );

            _coro = std::coroutine_handle<>();
          }

          if constexpr ( !std::is_same_v<void,SigRet> ) {
            return {};
          }
        }

        signal_awaiter_ret_helper<SigArgs...> _returnContainer;
        sigc::connection _conn;
        std::coroutine_handle<> _coro;

    };
  }

}

/**
  * This overload allows us to await a signal:
  *
  * \code
  * zyppng::Timer::Ptr t1 = zyppng::Timer::create();
  * t1->start(1000);
  * co_await t->sigExpired();
  * \endcode
  *
  * If the signal has arguments, they are returned via a std::tuple.
  * So for the signals signature: int signal(double, const std::string &), the co_await
  * would return a std::tuple<double, const std::string &>
  */
template< typename R, typename ...Args >
auto operator co_await( zyppng::SignalProxy<R(Args...)> &&signal ) noexcept
{
  return zyppng::detail::SignalAwaiter<R(Args...)>( std::move(signal) );
}

namespace zyppng::operators {

  //return a async op that waits for a signal to emitted by a object
  template <typename T,
            typename SignalGetter >
  auto await ( SignalGetter &&sigGet )
  {
    auto handler = [ sigGet = std::forward<SignalGetter>(sigGet) ]( std::shared_ptr<T> &&req  ) -> Task<std::shared_ptr<T>> {
      co_await std::invoke( sigGet, req );
      co_return req;
    };
    return handler;
  }
}

#endif
