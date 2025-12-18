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
#ifndef ZYPPNG_CORE_NG_ASYNC_TASK_H_INCLUDED
#define ZYPPNG_CORE_NG_ASYNC_TASK_H_INCLUDED

#include <zypp-core/ng/async/task.h>
#include <zypp-core/ng/base/signals.h>
#include <zypp-core/ng/base/eventloop.h>
#include <optional>

namespace zyppng {

  /**
   * \brief A helper class to bridge asynchronous IO callbacks with C++20 coroutines.
   *
   * The IOTaskAwaiter simplifies the implementation of awaitables for IO-bound operations
   * (such as Timers, Sockets, or File IO) that rely on an Event Loop. It acts as the
   * synchronization point between the low-level callback mechanism and the high-level
   * `Task<T>` coroutine return types.
   *
   * This class manages the storage of the result and the suspension/resumption of the
   * calling coroutine. It ensures that the coroutine is resumed safely via the
   * EventDispatcher, avoiding stack corruption issues that can occur if a coroutine
   * is resumed synchronously from within a destructor or a deeply nested callback.
   *
   * \tparam T The type of the value produced by the asynchronous operation.
   *
   * \note This class is non-copyable and non-movable to ensure the coroutine handle
   * and result storage remain stable in memory during the suspension period.
   *
   * \par Example
   * Implementing an asynchronous read operation using `Task<T>`:
   * \code
   * class Socket {
   * IOTaskAwaiter<std::string>* _activeRequest = nullptr;
   *
   * public:
   * // Public API returns a high-level Task
   * Task<std::string> async_read() {
   * // 1. Create the awaiter locally. It lives in this coroutine frame.
   * IOTaskAwaiter<std::string> awaiter;
   * * // 2. Register it so the IO callback can find it.
   * _activeRequest = &awaiter;
   *
   * // 3. Suspend execution here until data arrives.
   * std::string result = co_await awaiter;
   *
   * // 4. Resumed safely by EventDispatcher. Cleanup and return.
   * _activeRequest = nullptr;
   * co_return result;
   * }
   *
   * // IO Callback triggered by OS/EventLoop
   * void on_data(std::string data) {
   * if (_activeRequest) {
   * // Wakes up the coroutine safely
   * _activeRequest->return_value(std::move(data));
   * }
   * }
   * };
   * \endcode
   */

  template <typename T>
  class IOTaskAwaiter {
    public:
      IOTaskAwaiter() = default;
      ~IOTaskAwaiter() {
        if ( _destroyCallback ) {
          _destroyCallback( await_ready() );
        }
      }
      IOTaskAwaiter(const IOTaskAwaiter &) = delete;
      IOTaskAwaiter(IOTaskAwaiter &&) = delete;
      IOTaskAwaiter &operator=(const IOTaskAwaiter &) = delete;
      IOTaskAwaiter &operator=(IOTaskAwaiter &&) = delete;
      using value_type = T;

      bool await_ready() const noexcept
      {
        return _resultStore.has_value();
      }

      void await_suspend(std::coroutine_handle<> cont)
      {
        _handle = std::move( cont );
      }

      value_type await_resume() {
        return std::move( _resultStore.value() );
      }

      void return_value( T && val ) {
        _resultStore = std::move(val);
        if ( _handle ) {
          // invoke right away when entering the event loop again
          // there might be a more perfomant solution, but we need to avoid cleaning
          // up the coroutine stack while we are still in code belonging to part of that stack
          // e.g. this objects member functions.
          zyppng::EventDispatcher::invokeAfter( [c = std::move(_handle)](){
            c.resume();
            return false;
          } , 0 );

          _handle = std::coroutine_handle<>();
        }
      }

      /**
       * \brief registerDestroyCallback
       * Registers a callback to be executed when the task is destroyed.
       *
       * \note a already existing notify callback will be replaced
       */
      template <typename Fun>
        requires std::is_same_v<void, std::invoke_result_t<Fun, bool>>
      void registerDestroyCallback ( Fun &&cb ) {
        _destroyCallback = std::forward<Fun>(cb);
      }

    private:
      std::optional<T> _resultStore;
      std::coroutine_handle<> _handle;
      std::function<void(bool)> _destroyCallback;
  };


  template <>
  class IOTaskAwaiter<void> {
    public:
      IOTaskAwaiter() = default;
      ~IOTaskAwaiter() {
        if ( _destroyCallback ) {
          _destroyCallback( await_ready() );
        }
      }
      IOTaskAwaiter(const IOTaskAwaiter &) = delete;
      IOTaskAwaiter(IOTaskAwaiter &&) = delete;
      IOTaskAwaiter &operator=(const IOTaskAwaiter &) = delete;
      IOTaskAwaiter &operator=(IOTaskAwaiter &&) = delete;
      using value_type = void;

      bool await_ready() const noexcept
      {
        return _isReady;
      }

      void await_suspend(std::coroutine_handle<> cont)
      {
        _handle = std::move( cont );
      }

      void await_resume() {
        return;
      }

      void return_void( ) {
        if ( _handle ) {
          // invoke right away when entering the event loop again
          // there might be a more perfomant solution, but we need to avoid cleaning
          // up the coroutine stack while we are still in code belonging to part of that stack
          // e.g. this objects member functions.
          zyppng::EventDispatcher::invokeAfter( [c = std::move(_handle)](){
            c.resume();
            return false;
          } , 0 );

          _handle = std::coroutine_handle<>();
        }
      }

      /**
       * \brief registerDestroyCallback
       * Registers a callback to be executed when the task is destroyed.
       *
       * \note a already existing notify callback will be replaced
       */
      template <typename Fun>
        requires std::is_same_v<void, std::invoke_result_t<Fun, bool>>
      void registerDestroyCallback ( Fun &&cb ) {
        _destroyCallback = std::forward<Fun>(cb);
      }


    private:
      bool _isReady = false;
      std::coroutine_handle<> _handle;
      std::function<void(bool)> _destroyCallback;
  };

}


#endif
