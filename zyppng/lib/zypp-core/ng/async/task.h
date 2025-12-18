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
#ifndef ZYPPNG_CORE_ASYNC_TASK_H_INCLUDED
#define ZYPPNG_CORE_ASYNC_TASK_H_INCLUDED

#include <zypp-core/ng/async/awaitable.h>

namespace zyppng {

  enum TaskState {
    Pending,  // initially suspended
    Running,  // task is doing work, might wait for another coroutine
    Finished  // task done
  };

  namespace detail {
    template < class ExecutionStrategy >
    consteval TaskState initialCoroTaskState() {
      if constexpr ( std::is_same_v<ExecutionStrategy, coro_lazy_execution> ) {
        return TaskState::Pending;
      } else {
        return TaskState::Running;
      }
    }
  }

  template <class Result, class ExecutionStrategy >
  class TaskPromise {
  public:

    friend class Task<Result, ExecutionStrategy>;

    TaskPromise() = default;
    virtual ~TaskPromise() =default;

    TaskPromise(TaskPromise &&) = delete;
    TaskPromise &operator=(TaskPromise &&) = delete;

    TaskPromise(const TaskPromise &) = delete;
    TaskPromise &operator=(const TaskPromise &) = delete;

    template <class Res = Result>
    void return_value( Res &&value) noexcept(std::is_nothrow_copy_constructible_v<Res>)
    {
      setReady( std::forward<Res>(value) );
    }

    void unhandled_exception() noexcept
    {
      std::exception_ptr ex = std::current_exception();
      try {
        std::rethrow_exception (ex);

      } catch ( const zypp::Exception &e ) {

        ERR << "Unhandled zypp exception in coro: " << e << std::endl;

      } catch ( const std::exception &e ) {

        ERR << "Unhandled std::exception in coro: " << e.what() << std::endl;

      } catch ( ... ) {
        ERR << "CAUGHT unknown exception in coro " << std::endl;
      }

      setReady( ex );
    }

    void setContinuation ( std::coroutine_handle<> cont ) {
      _continuation = std::move(cont);
    }

    template <typename T = Result> void setReady(T &&value) {
      _result = std::forward<T>(value);
    }

    void setReady ( std::exception_ptr excp ) {
      if constexpr( zyppng::is_instance_of_v<zyppng::expected, Result> ) {
        _result = ( Result::error( excp ) );
      } else {
        _result = excp;
      }
    }

    zyppng::Task<Result, ExecutionStrategy> get_return_object() noexcept
    {
      zyppng::Task<Result, ExecutionStrategy> t;
      t._coroHandle = std::coroutine_handle<TaskPromise>::from_promise(*this);
      return t;
    }

    auto initial_suspend() noexcept {
      if constexpr ( std::is_same_v<ExecutionStrategy, coro_lazy_execution> ) {
        return std::suspend_always{};
      } else {
        return std::suspend_never{};
      }
    }

    auto final_suspend() noexcept {

      _state = TaskState::Finished;

      if constexpr ( std::is_same_v<ExecutionStrategy, coro_lazy_execution> ) {

        // symmetric transfer back to the coroutine that is waiting for us.
        struct TransferAwaiter {
          std::coroutine_handle<> continuation;
          std::function<void()> notify;
          bool await_ready() noexcept { return false; }

          std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {

            // trigger notify , Task<> is single ownership only so there is either a co_await
            // waiting for it OR some code using the notifyCallback to handle its lifetime
            // it's not possible that those two cases happen at the same time

            if ( notify )
              notify();

            // return the handle that is waiting for us, or noop_coroutine()
            if ( continuation )
              return continuation;

            return std::noop_coroutine ();
          }

          void await_resume() noexcept {}
        };

        return TransferAwaiter{ std::move(_continuation), std::move(_notifyCallback) };

      } else {

        if ( _notifyCallback )
          _notifyCallback();

        // suspend so that Task<> can clean up after us
        return std::suspend_always{};
      }
    }

    /*!
     * \brief taskStarted
     * Sets the internal state tracker to started
     */
    void taskStarted() {
      _state = TaskState::Running;
    }

    TaskState state() const {
      return _state;
    }

    private:
    TaskState _state = detail::initialCoroTaskState<ExecutionStrategy>();
    std::variant<std::monostate, Result, std::exception_ptr> _result;
    std::coroutine_handle<> _continuation;
    std::function<void()> _notifyCallback;
  };



  template <class ExecutionStrategy>
  class TaskPromise<void, ExecutionStrategy> {
  public:

    using TaskType = Task<void, ExecutionStrategy>;
    using Result = void;

    friend class Task<Result ,ExecutionStrategy>;

    class ReadyState{};


    TaskPromise() = default;
    virtual ~TaskPromise() = default;

    TaskPromise(TaskPromise &&) = delete;
    TaskPromise &operator=(TaskPromise &&) = delete;

    TaskPromise(const TaskPromise &) = delete;
    TaskPromise &operator=(const TaskPromise &) = delete;

    TaskType get_return_object();

    void return_void( )
    {
      setReady();
    }

    void unhandled_exception() noexcept
    {
      setReady( std::current_exception() );
    }

    void setContinuation ( std::coroutine_handle<> cont ) {
      _continuation = std::move(cont);
    }

    void setReady() {
      _result = ReadyState{};
    }

    void setReady ( std::exception_ptr excp ) {
      _result = excp;
    }

    auto initial_suspend() noexcept {
      if constexpr ( std::is_same_v<ExecutionStrategy, coro_lazy_execution> ) {
        return std::suspend_always{};
      } else {
        return std::suspend_never{};
      }
    }

    auto final_suspend() noexcept {

      _state = TaskState::Finished;

      if constexpr ( std::is_same_v<ExecutionStrategy, coro_lazy_execution> ) {        // symmetric transfer back to the coroutine that is waiting for us.
        struct TransferAwaiter {
          std::coroutine_handle<> continuation;
          std::function<void()> notify;
          bool await_ready() noexcept { return false; }

          std::coroutine_handle<> await_suspend(std::coroutine_handle<> h) noexcept {

            // trigger notify , Task<> is single ownership only so there is either a co_await
            // waiting for it OR some code using the notifyCallback to handle its lifetime
            // it's not possible that those two cases happen at the same time

            if ( notify )
              notify();

            // return the handle that is waiting for us, or noop_coroutine()
            if ( continuation )
              return continuation;

            return std::noop_coroutine ();
          }

          void await_resume() noexcept {}
        };

        return TransferAwaiter{ std::move(_continuation), std::move(_notifyCallback) };

      } else {

        if ( _notifyCallback )
          _notifyCallback();

        // suspend so that Task<> can clean up after us
        return std::suspend_always{};
      }
    }

    void taskStarted() {
      _state = TaskState::Running;
    }

    TaskState state() const {
      return _state;
    }

  protected:
    TaskState _state = detail::initialCoroTaskState<ExecutionStrategy>();
    std::variant<std::monostate, ReadyState, std::exception_ptr> _result;
    std::coroutine_handle<> _continuation;
    std::function<void()> _notifyCallback;
  };


  /*!
   * \brief A generic RAII wrapper for a C++20 coroutine handle.
   *
   * \details
   * The `Task` class serves as the return object for coroutine functions. It acts
   * as a unique owner of the underlying `std::coroutine_handle`. When the `Task`
   * goes out of scope, the coroutine frame is destroyed.
   *
   * **Key Concepts:**
   * - **Ownership:** This class is non-copyable but movable. It enforces unique
   * ownership of the coroutine frame to prevent double-free errors or memory leaks.
   * - **Lazy vs. Eager:** Depending on the \p ExecutionStrategy, the coroutine
   * may need to be manually started using the \ref start() method.
   * - **Result Handling:** It acts similarly to a `std::future`, storing either
   * the result of the computation or an exception pointer if the coroutine
   * threw an unhandled exception.
   *
   * \tparam Result The type of the value computed by the coroutine.
   * \tparam ExecutionStrategy A policy type (e.g., `coro_lazy_execution`) that
   * determines if the coroutine suspends immediately upon creation or
   * runs until the first suspension point.
   */
  template <class Result, class ExecutionStrategy>
  class Task {

    public:
      friend class TaskPromise<Result, ExecutionStrategy>;
      using execution_strategy = ExecutionStrategy;
      using promise_type = TaskPromise<Result, ExecutionStrategy>;
      using value_type   = Result;

    private:
      std::coroutine_handle<promise_type> _coroHandle;
      Task () = default;

    public:

      // no copy
      Task ( const Task &other ) = delete;
      Task& operator= ( const Task &other ) = delete;

      Task(Task&& other) noexcept : _coroHandle(other._coroHandle) {
        other._coroHandle = {};
      }

      Task& operator=(Task&& other) noexcept {
        if ( this == &other )
          return *this;
        std::swap( _coroHandle, other._coroHandle );
        return *this;
      }

      ~Task(){
        if (_coroHandle) _coroHandle.destroy();
      }

      void start () {
        if constexpr ( std::is_same_v< execution_strategy, coro_lazy_execution> ){
          if ( !_coroHandle
               || _coroHandle.done ()
               || _coroHandle.promise().state() != TaskState::Pending )
            return;

          // manually set to started, likely there will be no co_await on this task
          _coroHandle.promise().taskStarted();
          _coroHandle.resume ();
        }
      }

      /*!
       * Checks if the async operation already has finished.
       */
      bool isReady () const {
        return ( _coroHandle.promise()._result.index() != 0 );
      }

      /**
       * \brief registerNotifyCallback
       *
       * Registers a callback to be executed when the task is ready,
       * if the task is ready right away the callback will be triggered immediately.
       *
       * This mechanism is primarily used by combinators like `coro_poll` to detect
       * completion of Tasks that are not being directly `co_await`ed.
       *
       * \param callback A callable object (void()) to execute upon completion.
       *
       * \warning **CRITICAL LIFETIME SAFETY**:
       * The callback is invoked synchronously inside the coroutine's `final_suspend` block.
       * At this point, the coroutine frame is valid but **still active** on the stack.
       *
       * **DO NOT** destroy this `Task` object synchronously inside the callback.
       * Doing so will destroy the stack frame while code is still executing within it
       * (Use-After-Free), leading to an immediate crash.
       *
       * \note **Recommended Pattern**:
       * To safely handle completion, the callback should **post** the notification to
       * the Event Loop rather than acting immediately. This "trampolining" ensures that
       * the `notifyCallback` returns, the coroutine suspends properly, and the `Task`
       * is only destroyed in a subsequent Event Loop cycle.
       *
       * \note a already existing notify callback will be replaced
       */
      template <typename Fun>
      requires std::is_same_v<void, std::invoke_result_t<Fun>>
      void registerNotifyCallback ( Fun &&cb ) {
        _coroHandle.promise()._notifyCallback = std::forward<Fun>(cb);
        if ( isReady () )
          _coroHandle.promise()._notifyCallback ();
      }

      /**
       * \brief clearNotifyCallback
       * Clear the notify callback, must be called when a previously
       * registered callback goes out of scope
       */
      void clearNotifyCallback () {
        _coroHandle.promise()._notifyCallback = std::function<void()>();
      }

      std::coroutine_handle<promise_type> handle() {
        return _coroHandle;
      }

      /**
       * \brief get
       * Returns the result of the task, will throw if the Task is not ready.
       */
      Result &get() {
        auto &promise = _coroHandle.promise();
        if ( std::holds_alternative<std::exception_ptr>(promise._result) ) {
          ZYPP_RETHROW(std::get<std::exception_ptr>(promise._result));
        }
        return std::get<Result>(promise._result);
      }

      const Result &get() const {
        const auto &promise = _coroHandle.promise();
        if ( std::holds_alternative<std::exception_ptr>(promise._result) ) {
          ZYPP_RETHROW(std::get<std::exception_ptr>(promise._result));
        }
        return std::get<const Result>(promise._result);
      }
  };


  /*!
   * \brief Specialization of Task for coroutines that return `void`.
   *
   * \details
   * This class mirrors the primary `Task` template but handles cases where the
   * coroutine produces side effects rather than a concrete value. The \ref get()
   * method exists purely to rethrow exceptions, as there is no value to return.
   *
   * \tparam ExecutionStrategy The strategy defining how the coroutine is executed.
   */
  template <class ExecutionStrategy>
  class Task<void, ExecutionStrategy> {

    public:
      friend class TaskPromise<void, ExecutionStrategy>;
      using execution_strategy = ExecutionStrategy;
      using promise_type = TaskPromise<void, ExecutionStrategy>;
      using value_type   = void;

    private:
      std::coroutine_handle<promise_type> _coroHandle;
      Task () = default;

    public:

      // no copy
      Task ( const Task &other ) = delete;
      Task& operator= ( const Task &other ) = delete;

      Task(Task&& other) noexcept : _coroHandle(other._coroHandle) {
        other._coroHandle = {};
      }

      Task& operator=(Task&& other) noexcept {
        if ( this == &other )
          return *this;
        std::swap( _coroHandle, other._coroHandle );
        return *this;
      }

      ~Task(){
        if (_coroHandle) _coroHandle.destroy();
      }

      void start () {
        if (   !_coroHandle
             || _coroHandle.done ()
             || _coroHandle.promise().state() != TaskState::Pending )
          return;

        _coroHandle.promise().taskStarted();
        _coroHandle.resume ();
      }

      /*!
       * Checks if the async operation already has finished.
       */
      bool isReady () const {
        return ( _coroHandle.promise()._result.index() != 0 );
      }

      /*!
       * Will rethrow any unhandled exception caught by the coroutine
       */
      void get() const {
        const auto &promise = _coroHandle.promise();
        if ( std::holds_alternative<std::exception_ptr>(promise._result) ) {
          ZYPP_RETHROW(std::get<std::exception_ptr>(promise._result));
        }
      }

      /**
       * \brief registerNotifyCallback
       *
       * Registers a callback to be executed when the task is ready,
       * if the task is ready right away the callback will be triggered immediately.
       *
       * This mechanism is primarily used by combinators like `coro_poll` to detect
       * completion of Tasks that are not being directly `co_await`ed.
       *
       * \param callback A callable object (void()) to execute upon completion.
       *
       * \warning **CRITICAL LIFETIME SAFETY**:
       * The callback is invoked synchronously inside the coroutine's `final_suspend` block.
       * At this point, the coroutine frame is valid but **still active** on the stack.
       *
       * **DO NOT** destroy this `Task` object synchronously inside the callback.
       * Doing so will destroy the stack frame while code is still executing within it
       * (Use-After-Free), leading to an immediate crash.
       *
       * \note **Recommended Pattern**:
       * To safely handle completion, the callback should **post** the notification to
       * the Event Loop rather than acting immediately. This "trampolining" ensures that
       * the `notifyCallback` returns, the coroutine suspends properly, and the `Task`
       * is only destroyed in a subsequent Event Loop cycle.
       *
       * \note a already existing notify callback will be replaced
       */
      template <typename Fun>
        requires std::is_same_v<void, std::invoke_result_t<Fun>>
      void registerNotifyCallback ( Fun &&cb ) {
        _coroHandle.promise()._notifyCallback = std::forward<Fun>(cb);
        if ( isReady () )
          _coroHandle.promise()._notifyCallback ();
      }

      /**
       * @brief clearNotifyCallback
       * Clear the notify callback, must be called when a previously
       * registered callback goes out of scope
       */
      void clearNotifyCallback () {
        _coroHandle.promise()._notifyCallback = std::function<void()>();
      }

      std::coroutine_handle<promise_type> handle() {
        return _coroHandle;
      }
  };


  template <class ExecutionStrategy>
  inline TaskPromise<void, ExecutionStrategy >::TaskType TaskPromise<void, ExecutionStrategy >::get_return_object() {
    TaskType obj;
    obj._coroHandle = std::coroutine_handle<TaskPromise<void, ExecutionStrategy>>::from_promise(*this);
    return obj;
  }

  template <typename T>
  Task<std::decay_t<T>> makeReadyTask( T result )
  {
    co_return std::move(result);
  }

}

namespace zyppng {

  template<class T, class Exec>
  auto operator co_await( Task<T, Exec> &&future ) noexcept
  {
    struct TaskAwaiter
    {
    public:
      using value_type = T;

      TaskAwaiter( Task<T, Exec> &&fut ) : _fut( std::move(fut) ) {}
      TaskAwaiter(const TaskAwaiter &) = delete;
      TaskAwaiter(TaskAwaiter &&) = delete;
      TaskAwaiter &operator=(const TaskAwaiter &) = delete;
      TaskAwaiter &operator=(TaskAwaiter &&) = delete;

      virtual ~TaskAwaiter() {
        if ( !_fut.isReady() ) {
          WAR << "Awaiter destroyed before Task became ready" << std::endl;
        }
      }

      bool await_ready() const noexcept
      {
        return _fut.isReady();
      }

      std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont)
      {
        if constexpr ( std::is_same_v< coro_lazy_execution, Exec> ) {

          typename Task<T,Exec>::promise_type &p = _fut.handle().promise ();

          switch( p.state() ) {

            case TaskState::Pending: {
              p.taskStarted();
              _fut.handle().promise().setContinuation( std::move(cont) );
              return _fut.handle();
            }
            case TaskState::Running: {
              // task is running, might be waiting do nothing
              return std::noop_coroutine();
            }
            case TaskState::Finished: {
              // no work to be done, directly resume the waiting coroutine
              return cont;
            }
            default: {
              ERR << "Must handle all task states!" << std::endl;
              std::terminate ();
              // never going to reach this part
              return std::noop_coroutine();
            }
          }

        } else {
          _fut.registerNotifyCallback ( [c = std::move(cont)] () mutable {
            EventDispatcher::instance ()->invokeAfter (
              [ c = std::move(c)](){
                c.resume();
                return false;
              }
              , 0);
          });
          return std::noop_coroutine();
        }
      }

      value_type await_resume() {
        if constexpr ( std::same_as<T, void> ) {
          return _fut.get();
        } else {
          return std::move(_fut.get());
        }
      }

    private:
      Task<T, Exec> _fut;
    };

    return TaskAwaiter { std::move(future) };
  }

}


#endif
