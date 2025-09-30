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
#ifndef ZYPPNG_AWAITABLE_H_INCLUDED
#define ZYPPNG_AWAITABLE_H_INCLUDED

#include <variant>
#include <zypp-core/ng/pipelines/expected.h>
#include <zypp-core/ng/meta/type_traits.h>
#include <zypp-core/ng/base/eventdispatcher.h>
#include <coroutine>

namespace zyppng {


  /*!
   * \brief The Task class
   */
  template <typename Result>
  class Task {

    public:
      class Promise;
      using promise_type = Promise;
      using result_type  = Result;

    private:
      struct SharedData {

          SharedData( Task &parent ) : _parent(parent) {}
          SharedData(const SharedData &) = delete;
          SharedData(SharedData &&) = delete;
          SharedData &operator=(const SharedData &) = delete;
          SharedData &operator=(SharedData &&) = delete;
          virtual ~SharedData() {
            if ( _destroyCallback )
              _destroyCallback();
          }

          template <typename T = Result> void setReady(T &&value) {
            _result = std::forward<T>(value);
            notify();
          }

          void setReady ( std::exception_ptr excp ) {
            if constexpr( zyppng::is_instance_of_v<zyppng::expected, Result> ) {
              _result = ( Result::error( excp ) );
            } else {
              _result = excp;
            }
            notify();
          }

          void notify() {
            if ( _notifyCallback )
              _notifyCallback();
          }

          std::reference_wrapper<Task> _parent;
          std::variant<std::monostate, Result, std::exception_ptr> _result;
          std::function<void()> _notifyCallback;
          std::function<void()> _destroyCallback;
      };

      std::shared_ptr<SharedData> _data;

      Task () : _data( std::make_shared<SharedData>(*this) ) {
      }

      Task( std::shared_ptr<SharedData> &&data ) : _data( std::move(data) ) {
        _data->_parent = *this;
      }

    public:

      // no copy
      Task ( const Task &other ) = delete;
      Task& operator= ( const Task &other ) = delete;

      Task& operator= ( Task &&other ) noexcept {
        this->_data = std::move(other._data);
        _data->_parent = std::ref(*this);
        return *this;
      }

      Task ( Task &&other ) noexcept
        : _data( std::move(other._data) ) {
        _data->_parent = std::ref(*this);
      }

      virtual ~Task() = default;


      /*!
       * Checks if the async operation already has finished.
       */
      bool isReady () const {
        return ( _data->_result.index() != 0 );
      }


      /**
       * \brief registerNotifyCallback
       * Registers a callback to be executed when the task is ready,
       * if the task is ready right away the callback will be triggered immediately.
       *
       * \note a already existing notify callback will be replaced
       */
      template <typename Fun>
      requires std::is_same_v<void, std::invoke_result_t<Fun>>
      void registerNotifyCallback ( Fun &&cb ) {
        _data->_notifyCallback = std::forward<Fun>(cb);
        if ( isReady () )
          _data->notify ();
      }

      /**
       * \brief registerNotifyCallback
       * Registers a callback to be executed when the task is destroyed.
       *
       * \note a already existing notify callback will be replaced
       */
      template <typename Fun>
      requires std::is_same_v<void, std::invoke_result_t<Fun>>
      void registerDestroyCallback ( Fun &&cb ) {
        _data->_destroyCallback = std::forward<Fun>(cb);
      }

      /**
       * \brief get
       * Returns the result of the task, will throw if the Task is not ready.
       */
      Result &get() {
        if ( std::holds_alternative<std::exception_ptr>(_data->_result) ) {
          ZYPP_RETHROW(std::get<std::exception_ptr>(_data->_result));
        }
        return std::get<Result>(_data->_result);
      }

      const Result &get() const {
        if ( std::holds_alternative<std::exception_ptr>(_data->_result) ) {
          ZYPP_RETHROW(std::get<std::exception_ptr>(_data->_result));
        }
        return std::get<const Result>(_data->_result);
      }


      class Promise {
        public:

          Promise() = default;
          virtual ~Promise() = default;

          Promise(Promise &&) = default;
          Promise &operator=(Promise &&) = default;

          Promise(const Promise &) = delete;
          Promise &operator=(const Promise &) = delete;

          bool valid() const {
            return (!_ret_obj.expired());
          }

          Task get_return_object() noexcept {

            Task obj;
            _ret_obj = obj._data;

            return obj;
          }

          std::suspend_never initial_suspend() const noexcept { return {}; }
          std::suspend_never final_suspend() const noexcept { return {}; }

          void return_value(const Result& value) noexcept(std::is_nothrow_copy_constructible_v<Result>)
          {
            if ( !valid() ) {
              WAR << "Returning a value from a expired Promise, ignoring" << std::endl;
              return;
            }
            _ret_obj.lock()->setReady( T(value) );
          }

          void return_value(Result&& value) noexcept(std::is_nothrow_move_constructible_v<Result>)
          {
            if ( !valid() ) {
              WAR << "Returning a value from a expired Promise, ignoring" << std::endl;
              return;
            }
            _ret_obj.lock()->setReady ( std::move(value) );
          }

          void unhandled_exception() noexcept
          {
            if ( !valid() ) {
              WAR << "Returning a exception from a expired Promise, ignoring" << std::endl;
              return;
            }
            _ret_obj.lock ()->setReady( std::current_exception() );
          }

        protected:
          std::weak_ptr<SharedData> _ret_obj;
      };
  };


  template <>
  class Task<void> {

    public:
      class Promise;
      using promise_type = Promise;
      using result_type  = void;

    private:
      struct SharedData {

          class ReadyState{};

          SharedData( Task &parent ) : _parent(parent){}
          SharedData(const SharedData &) = delete;
          SharedData(SharedData &&) = delete;
          SharedData &operator=(const SharedData &) = delete;
          SharedData &operator=(SharedData &&) = delete;
          virtual ~SharedData() {
            if ( _destroyCallback )
              _destroyCallback();
          }

          void setReady() {
            _result = ReadyState();
            notify();
          }

          void setReady ( std::exception_ptr excp ) {
            _result = excp;
            notify();
          }

          void notify() {
            if ( _notifyCallback )
              _notifyCallback();
          }

          std::reference_wrapper<Task> _parent;
          std::variant<std::monostate, ReadyState, std::exception_ptr> _result;
          std::function<void()> _notifyCallback;
          std::function<void()> _destroyCallback;
      };

      std::shared_ptr<SharedData> _data;

      Task () : _data( std::make_shared<SharedData>(*this) ) {
      }

      Task( std::shared_ptr<SharedData> &&data ) : _data( std::move(data) ) {
        _data->_parent = *this;
      }

    public:

      // no copy
      Task ( const Task &other ) = delete;
      Task& operator= ( const Task &other ) = delete;

      Task& operator= ( Task &&other ) noexcept {
        this->_data = std::move(other._data);
        _data->_parent = std::ref(*this);
        return *this;
      }

      Task ( Task &&other ) noexcept
        : _data( std::move(other._data) ) {
        _data->_parent = std::ref(*this);
      }

      virtual ~Task() = default;


      /*!
       * Checks if the async operation already has finished.
       */
      bool isReady () const {
        return ( _data->_result.index() != 0 );
      }

      /*!
       * Will rethrow any unhandled exception caught by the coroutine
       */
      void get() const {
        if ( std::holds_alternative<std::exception_ptr>(_data->_result) ) {
          ZYPP_RETHROW(std::get<std::exception_ptr>(_data->_result));
        }
      }


      /**
       * \brief registerNotifyCallback
       * Registers a callback to be executed when the task is ready,
       * if the task is ready right away the callback will be triggered immediately.
       *
       * \note a already existing notify callback will be replaced
       */
      template <typename Fun>
      requires std::is_same_v<void, std::invoke_result_t<Fun>>
      void registerNotifyCallback ( Fun &&cb ) {
        _data->_notifyCallback = std::forward<Fun>(cb);
        if ( isReady () )
          _data->notify ();
      }

      /**
       * \brief registerNotifyCallback
       * Registers a callback to be executed when the task is destroyed.
       *
       * \note a already existing notify callback will be replaced
       */
      template <typename Fun>
      requires std::is_same_v<void, std::invoke_result_t<Fun>>
      void registerDestroyCallback ( Fun &&cb ) {
        _data->_destroyCallback = std::forward<Fun>(cb);
      }


      class Promise {
        public:
          Promise() = default;
          virtual ~Promise() = default;

          Promise(Promise &&) = default;
          Promise &operator=(Promise &&) = default;

          Promise(const Promise &) = delete;
          Promise &operator=(const Promise &) = delete;

          Task get_return_object() noexcept {

            Task obj;
            _ret_obj = obj._data;

            return obj;
          }

          std::suspend_never initial_suspend() const noexcept { return {}; }
          std::suspend_never final_suspend() const noexcept { return {}; }

          void return_void()
          {
            _ret_obj.lock()->setReady();
          }

          void unhandled_exception() noexcept
          {
            _ret_obj.lock ()->setReady( std::current_exception() );
          }

        protected:
          std::weak_ptr<SharedData> _ret_obj;
      };
  };


  template <typename T>
  struct TaskAwaiter
  {
    TaskAwaiter(Task<T> fut) : _fut(std::move(fut)) {}
    TaskAwaiter(const TaskAwaiter &) = delete;
    TaskAwaiter(TaskAwaiter &&) = delete;
    TaskAwaiter &operator=(const TaskAwaiter &) = delete;
    TaskAwaiter &operator=(TaskAwaiter &&) = delete;

    ~TaskAwaiter() {
      if ( !_fut.isReady() ) {
        WAR << "Awaiter destroyed before Task became ready" << std::endl;
      }
    }

    bool await_ready() const noexcept
    {
      return _fut.isReady();
    }

    void await_suspend(std::coroutine_handle<> cont)
    {
      _fut.registerNotifyCallback ( [c = std::move(cont)] () mutable {
        // invoke right away when entering the event loop again
        zyppng::EventDispatcher::invokeAfter( [c = std::move(c)](){
          c.resume();
          return false;
        } , 0 );
      } );
    }

    T await_resume() {
      return _fut.get();
    }

    Task<T> _fut;
  };

}

// Allow co_await'ing AsyncOpRef<T>
template<typename T>
auto operator co_await( zyppng::Task<T> &&future ) noexcept
requires(!std::is_reference_v<T>)
{
  return zyppng::TaskAwaiter { std::move(future) };
}

namespace zyppng::operators {

  template< typename TaskRes , typename Callback>
  requires ( std::is_invocable_v<Callback, TaskRes> )
  Task<std::invoke_result_t<Callback,TaskRes>> operator| ( Task<TaskRes> &&in, Callback &&c )
  {
    auto r = co_await std::move(in);
    if constexpr( is_instance_of_v< Task, std::invoke_result_t<Callback, decltype(r)>  >) {
      co_return ( co_await(c(std::move(r))) );
    } else {
      co_return c(std::move(r));
    }
  }
}

#endif
