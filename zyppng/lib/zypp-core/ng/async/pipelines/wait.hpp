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
*/

#ifndef ZYPP_ZYPPNG_MONADIC_WAIT_H
#define ZYPP_ZYPPNG_MONADIC_WAIT_H

#include <zypp-core/ng/async/task.h>
#include <zypp-core/ng/async/iotask.h>

namespace zyppng {

  /*!
   * \brief coro_poll
   * Polls a vector of parallel running Tasks for completion. Loosely modeled after poll(2).
   * If the vector contains already finished Tasks the function returns immediately.
   *
   * \return Returns the number of Tasks in the vector that are ready
   * \note this function is using the notifyCallback feature of Task, all previously installed callbacks will be removed.
   */
  template < typename T, typename Exec >
  Task<int> coro_poll( std::vector< Task<T, Exec> * > tasks  ) {
    struct CoroPollImpl : public IOTaskAwaiter<int> {
      CoroPollImpl( std::vector< Task<T, Exec> * > &&t ) : _tasks(std::move(t)) {}

      CoroPollImpl(CoroPollImpl &&) = delete;
      CoroPollImpl &operator=(CoroPollImpl &&) = delete;

      CoroPollImpl(const CoroPollImpl &) = delete;
      CoroPollImpl &operator=(const CoroPollImpl &) = delete;

      ~CoroPollImpl(){
        std::for_each( _tasks.begin(), _tasks.end(), []( auto &t ) { t->clearNotifyCallback(); });
      }


      void await_suspend(std::coroutine_handle<> cont)
      {
        IOTaskAwaiter<int>::await_suspend( std::move(cont) );

        _settingUp = true;
        for ( Task<T, Exec>* t : _tasks ) {
          if constexpr ( std::is_same_v<Exec, coro_lazy_execution> ) {
            t->start();
          }
          t->registerNotifyCallback( [this](){
            triggered();
          });
        }
        _settingUp = false;
        triggered (); // just in case
      }

      void triggered() {
        if ( _settingUp )
          return;

        int ready = 0;
        for ( Task<T, Exec>* t : _tasks ) {
          if ( t->isReady() ) {
            ready++;
          }
        }
        if ( ready > 0 ) {
          std::for_each( _tasks.begin(), _tasks.end(), []( auto &t ) { t->clearNotifyCallback(); });
          _tasks.clear();

          // will return the value via a event through the eventloop
          // so we cannot cut the branch we are currently sitting on
          this->return_value (int(ready));
        }
      }

      private:
        bool _settingUp = false;
        std::vector< Task<T, Exec> * > _tasks;
    };

    CoroPollImpl awaiter { std::move(tasks) };

    co_return co_await( awaiter );
  }


  template < typename T, typename Exec >
  Task<int> coro_poll( std::initializer_list< Task<T, Exec> * > task_list  ) {
    return coro_poll( std::vector< Task<T, Exec> * > ( task_list.begin (), task_list.end() ) );
  }



namespace detail {
  struct JoinHelper
  {
      template<
          template< class, class... > class Container,
          class T,
          class E,
          class ...CArgs
          >
      requires( !std::is_void_v<T> )
      Task<Container<T>> operator()( Container< Task<T, E>, CArgs...> ops ) {

        while ( true ) {
          std::vector<Task<T,E> *> pending;
          std::for_each( ops.begin(), ops.end(), [&]( auto &t ) { if(!t.isReady()){ pending.push_back(&t); } });
          if ( pending.empty () )
            break;

          auto ready = co_await coro_poll( std::move(pending) );
          if ( ready == 0 ) {
            DBG << "coro_poll returned without ready elems" << std::endl;
            continue;
          }
        }

        Container<T> res;
        std::for_each( ops.begin(), ops.end(), [&]( auto &t ) { res.push_back(std::move(t.get())); });
        co_return res;
      }
  };
}

namespace operators {
  /*!
   *  Returns a async operation that waits for all async operations which are passed to it and collects their results,
   *  forwarding them as one
   */
  inline auto join ( ) {
    return detail::JoinHelper();
  }
}

}
#endif
