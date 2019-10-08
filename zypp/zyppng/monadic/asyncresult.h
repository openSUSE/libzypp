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
#ifndef ZYPPNG_MONADIC_ASYNCRESULT_H_INCLUDED
#define ZYPPNG_MONADIC_ASYNCRESULT_H_INCLUDED

#include <zypp/zyppng/meta/TypeTraits>
#include <zypp/zyppng/meta/FunctionTraits>
#include <zypp/zyppng/meta/Functional>

#include <zypp/base/Exception.h>
#include <zypp/base/Debug.h>


#include <functional>
#include <memory>
#include <boost/optional.hpp>

namespace zyppng {

  /*!
   * Thrown if code tries to access the result of a async operation
   * that is not ready.
   */
  class AsyncOpNotReadyException : public zypp::Exception
  {
  public:
    AsyncOpNotReadyException()
      : Exception( "AsyncOp instance not ready" )
    {}
    virtual ~AsyncOpNotReadyException();
  };

  AsyncOpNotReadyException::~AsyncOpNotReadyException()
  { }

  /*!
   *\class AsyncOp
   * The \a AsyncOp template class is the basic building block for the asynchronous pipelines.
   * Its the base for all async callbacks as well as the async result type. That basically means
   * every pipeline is just a AsyncOp that contains all previous operations that were defined in the pipeline.
   *
   * When implementing a async operation it is required to add a operator() to the class taking the
   * input parameter. After the operation is finished the implementation must call setReady(). Calling
   * setReady() must be treated like calling return in a normal function and not execute anymore code on the
   * AsyncOp instance afterwards, since the next operation in the pipeline is allowed to free the previous operation
   * as soon as it gets ready.
   *
   * In case no next operation is defined on the AsyncOp ( when the instance is used as result object ) the return value
   * is cached internally and can be retrieved with \sa get().
   *
   * A async operation can be cancelled by releasing the result object ( the resulting combinatory object ), this will
   * destroy all previous operations that are either running or pending as well.
   */
  template <typename Result>
  struct AsyncOp {

    using value_type = Result;

    AsyncOp () = default;

    AsyncOp ( const AsyncOp &other ) = delete;
    AsyncOp& operator= ( const AsyncOp &other ) = delete;

    AsyncOp& operator= ( AsyncOp &&other ) = default;
    AsyncOp ( AsyncOp &&other ) = default;

    virtual ~AsyncOp(){}

    /*!
     * Sets the async operation ready, in case a callback
     * is registered the \a val is directly forwarded without
     * storing it.
     */
    void setReady ( value_type && val ) {
      if ( _readyCb )
        _readyCb( std::move( val ) );
      else //we need to cache the value because no callback is available
        _maybeValue = std::move(val);
    }

    /*!
     * Checks if the async operation already has finished.
     *
     * \note This can only be used when no callback is registered.
     */
    bool isReady () const {
      return _maybeValue.is_initialized();
    }

    /*!
     * Registeres a callback that is immediately called
     * when the object gets into ready state. In case the
     * object is in ready state when registering the callback
     * it is called right away.
     */
    template< typename Fun >
    void onReady ( Fun cb ) {
      this->_readyCb =  std::forward<Fun>(cb);

      if ( isReady() ) {
        _readyCb( std::move( _maybeValue.get()) );
        _maybeValue = boost::optional<value_type>();
      }
    }

    /*!
     * Returns the internally cached value if there is one.
     * \throws AsyncOpNotReadyException if called before the operation is ready
     */
    value_type &get (){
      if ( !isReady() )
        ZYPP_THROW(AsyncOpNotReadyException());
      return _maybeValue.get();
    }

  private:
    std::function<void(value_type &&)> _readyCb;
    boost::optional<value_type> _maybeValue;
  };

  template <typename T>
  using AsyncOpPtr = std::unique_ptr<AsyncOp<T>>;

  namespace detail {

    template <typename T>
    using has_value_type_t = typename T::value_type;

    template <typename T, typename AsyncRet>
    using is_asyncop_type = std::is_convertible<T*, AsyncOp<AsyncRet>*>;

#if 0

    template <typename T>
    using is_async_op = typename std::conjunction<
        std::is_detected< has_value_type_t, T >,
        std::is_detected< is_asyncop_type, T, typename T::value_type >
      >;

#else

    /*!
     * Checks if a type T is a asynchronous operation type
     */
    template < typename T, typename = std::void_t<> >
    struct is_async_op : public std::false_type{};

    template < typename T >
    struct is_async_op<T,
      std::void_t< typename T::value_type, //needs to have a value_type
      std::enable_if_t<std::is_convertible< T*, AsyncOp<typename T::value_type>* >::value>>> //needs to be convertible to AsyncOp<T::value_type>
      : public std::true_type{};

#endif

    /*!
     * Checks if a Callable object is of type async_op and if
     * it accepts the given MsgType and returns the expected type
     */
    template< typename Callback, typename Ret, typename MsgType, typename = std::void_t<> >
    struct is_future_monad_cb : public std::false_type{};

    template< typename Callback, typename Ret, typename MsgType >
    struct is_future_monad_cb<Callback, Ret, MsgType,
      std::void_t<
        typename Callback::value_type,
        std::enable_if_t< std::is_same< typename Callback::value_type, Ret >::value >,
        std::enable_if_t< is_async_op<Callback>::value >,
        decltype ( std::declval<Callback>()( std::declval<MsgType>()) )//check if the callback has a operator() member with the correct signature
        >
      > : public std::true_type{};

    /*!
     * Checks if a Callable object is a syncronous callback type with the expected signature
     */
    template< typename Callback, typename MsgType, typename = std::void_t<> >
    struct is_sync_monad_cb : public std::false_type{};

    template< typename Callback, typename MsgType >
    struct is_sync_monad_cb<Callback, MsgType
      , std::void_t<
        std::enable_if_t< !is_async_op<Callback>::value >,
        std::enable_if_t< !std::is_same< void, decltype ( std::declval<Callback>()(std::declval<MsgType>())) >::value > > //check if the callback has the correct signature:  cb( MsgType )
      > : public std::true_type{};


    /*!
     * The AsyncResult class is used to bind previous operations to a new operation and to carry
     * the result of the full pipeline. It has a pointer to the _prevTask, which is usually
     * a AsyncResult too, and to the task it should execute once the previous task enters ready state.
     *
     * In theory this is a single linked list, but each node owns all the previous nodes, means that once
     * a node is destroyed all previous ones are destroyed as well. Basically the async results are nested objects, where
     * the outermost object is the last to be executed. While executing the nodes they are cleaned up right away
     * after they enter finished or ready state.
     *
     * When returned to the code the AsyncResult is casted into the AsyncOp base class, otherwise the type
     * information gets too complex and matching the pipe operator functions is a nightmare. This could be revisited
     * with newer C++ versions.
     *
     */
    template <typename Prev, typename AOp, typename = void>
    struct AsyncResult;

    template <typename Prev, typename AOp >
    struct AsyncResult<Prev,AOp> : public zyppng::AsyncOp< typename AOp::value_type > {

      AsyncResult ( std::unique_ptr<Prev> && prevTask, std::unique_ptr<AOp> &&cb )
        : _prevTask( std::move(prevTask) )
        , _myTask( std::move(cb) ) {
        connect();
      }

      AsyncResult ( const AsyncResult &other ) = delete;
      AsyncResult& operator= ( const AsyncResult &other ) = delete;

      AsyncResult ( AsyncResult &&other ) = delete;
      AsyncResult& operator= ( AsyncResult &&other ) = delete;

      virtual ~AsyncResult() {}

      void connect () {
        //not using a lambda here on purpose, binding this into a lambda that is stored in the _prev
        //object causes segfaults on gcc when the lambda is cleaned up with the _prev objects signal instance
        _prevTask->onReady( std::bind( &AsyncResult::readyWasCalled, this, std::placeholders::_1) );
        _myTask->onReady( [this] ( typename AOp::value_type && res ){
          this->setReady( std::move( res ) );
        });
      }

    private:
      void readyWasCalled ( typename Prev::value_type && res ) {
        //we need to force the passed argument into our stack, otherwise we
        //run into memory issues if the argument is moved out of the _prevTask object
        typename Prev::value_type resStore = std::move(res);

        if ( _prevTask ) {
          //dumpInfo();
          _prevTask.reset(nullptr);
        }

        _myTask->operator()(std::move(resStore));
      }

      std::unique_ptr<Prev> _prevTask;
      std::unique_ptr<AOp> _myTask;
    };

    template<typename AOp, typename In>
    struct AsyncResult<void, AOp, In> : public zyppng::AsyncOp< typename AOp::value_type > {

      AsyncResult ( std::unique_ptr<AOp> &&cb )
        : _myTask( std::move(cb) ) {
        connect();
      }

      virtual ~AsyncResult() { }

      void run ( In &&val ) {
        _myTask->operator()( std::move(val) );
      }

      AsyncResult ( const AsyncResult &other ) = delete;
      AsyncResult& operator= ( const AsyncResult &other ) = delete;

      AsyncResult ( AsyncResult &&other ) = delete;
      AsyncResult& operator= ( AsyncResult &&other ) = delete;

    private:
      void connect () {
        _myTask->onReady( [this] ( typename AOp::value_type && in ){
          this->setReady( std::move( in ) );
        });
      }
      std::unique_ptr<AOp> _myTask;
    };

    //A async result that is ready right away
    template <typename T>
    struct ReadyResult : public zyppng::AsyncOp< T >
    {
      ReadyResult( T &&val ) {
        this->setReady( std::move(val) );
      }
    };

    //need a wrapper to connect a sync callback to a async one
    template < typename Callback, typename In, typename Out >
    struct SyncCallbackWrapper : public AsyncOp<Out>
    {
      using value_type = Out;

      template< typename C = Callback >
      SyncCallbackWrapper( C &&c ) : _c( std::forward<C>(c) ){}

      virtual ~SyncCallbackWrapper(){ }

      void operator() ( In &&value ) {
        this->setReady( std::invoke( _c, std::move(value)) );
      }

    private:
      Callback _c;
    };

    /*!
     * Simple AsyncOperator wrapper that accepts a Async result
     * forwarding the actual value when it gets ready. This is used to
     * simplify nested asyncronous results.
     */
    template< typename AOp, typename Ret = typename AOp::value_type >
    struct SimplifyHelper : public AsyncOp<Ret>
    {

      virtual ~SimplifyHelper(){}

      void operator() ( std::unique_ptr<AOp> &&op ) {
        assert( !_task );
        _task = std::move(op);
        _task->onReady( [this]( Ret &&val ){
          this->setReady( std::move(val) );
        });
      }
    private:
      std::unique_ptr<AOp> _task;
    };

    /*!
     * Takes a possibly nested future and simplifies it,
     * so instead of AsyncResult<AsyncResult<int>> we get AsyncResult<int>.
     * Usually we do not want to wait on the future of a future but get the nested result immediately
     */
    template < typename Res >
    inline std::unique_ptr<AsyncOp<Res>> simplify ( std::unique_ptr< AsyncOp<Res> > &&res ) {
      return std::move(res);
    }

    template < typename Res,
      typename AOp =  AsyncOp< std::unique_ptr< AsyncOp<Res>> > >
    inline std::unique_ptr<AsyncOp<Res>> simplify ( std::unique_ptr< AsyncOp< std::unique_ptr< AsyncOp<Res>> > > &&res ) {
      std::unique_ptr<AsyncOp<Res>> op = std::make_unique< detail::AsyncResult< AOp, SimplifyHelper< AsyncOp<Res>>> >( std::move(res), std::make_unique<SimplifyHelper< AsyncOp<Res>>>() );
      return detail::simplify( std::move(op) );
    }
  }

  template <typename T>
  AsyncOpPtr<T> makeReadyResult ( T && result ) {
    return std::make_unique<detail::ReadyResult<T>>( std::move(result) );
  }

  namespace operators {

    //case 1 : binding a async message to a async callback
    template< typename PrevOp
      , typename Callback
      , typename Ret =  typename Callback::value_type
      , std::enable_if_t< detail::is_async_op<PrevOp>::value, int> = 0
      //is the callback signature what we want?
      , std::enable_if_t< detail::is_future_monad_cb< Callback, Ret, typename PrevOp::value_type >::value, int> = 0
      >
    auto operator| ( std::unique_ptr<PrevOp> &&future, std::unique_ptr<Callback> &&c )
    {
      std::unique_ptr<AsyncOp<Ret>> op = std::make_unique<detail::AsyncResult< PrevOp, Callback>>( std::move(future), std::move(c) );
      return detail::simplify( std::move(op) );
    }

    //case 2: binding a async message to a sync callback
    template< typename PrevOp
      , typename Callback
      , typename Ret = std::result_of_t< Callback( typename PrevOp::value_type&& ) >
      , std::enable_if_t< detail::is_async_op<PrevOp>::value, int> = 0
      , std::enable_if_t< detail::is_sync_monad_cb< Callback, typename PrevOp::value_type >::value, int> = 0
      >
    auto operator| ( std::unique_ptr<PrevOp> &&future, Callback &&c )
    {
      std::unique_ptr<AsyncOp<Ret>> op(std::make_unique<detail::AsyncResult< PrevOp, detail::SyncCallbackWrapper<Callback, typename PrevOp::value_type, Ret> >>(
        std::move(future)
          ,  std::make_unique<detail::SyncCallbackWrapper<Callback, typename PrevOp::value_type, Ret>>( std::forward<Callback>(c) ) ));

      return detail::simplify( std::move(op) );
    }

    //case 3: binding a sync message to a async callback
    template< typename SyncRes
      , typename Callback
      , typename Ret = typename Callback::value_type
      , std::enable_if_t< !detail::is_async_op< remove_smart_ptr_t<SyncRes> >::value, int> = 0
      , std::enable_if_t< detail::is_future_monad_cb< Callback, Ret, SyncRes >::value, int> = 0
      >
    auto  operator| ( SyncRes &&in, std::unique_ptr<Callback> &&c )
    {
      AsyncOpPtr<Ret> op( std::make_unique<detail::AsyncResult<void, Callback, SyncRes>>( std::move(c) ) );
      static_cast< detail::AsyncResult<void, Callback, SyncRes>* >(op.get())->run( std::move(in) );
      return detail::simplify( std::move(op) );
    }

    //case 4: binding a sync message to a sync callback
    template< typename SyncRes
      , typename Callback
      , std::enable_if_t< !detail::is_async_op< remove_smart_ptr_t<SyncRes> >::value, int > = 0
      , std::enable_if_t< detail::is_sync_monad_cb< Callback, SyncRes >::value, int > = 0
      >
    auto operator| ( SyncRes &&in, Callback &&c )
    {
      return std::forward<Callback>(c)(std::forward<SyncRes>(in));
    }

  }
}

#endif
