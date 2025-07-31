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

#include <zypp-core/ng/meta/TypeTraits>
#include <zypp-core/ng/meta/FunctionTraits>
#include <zypp-core/ng/meta/Functional>
#include <zypp-core/ng/async/AsyncOp>

namespace zyppng {


  namespace detail {

    template< typename Callback, typename MsgType, typename = std::void_t<> >
    struct is_future_monad_cb : public std::false_type{};

    template< typename Callback, typename MsgType >
    struct is_future_monad_cb<Callback, MsgType,
      std::void_t<
        std::enable_if_t< is_async_op_v<Callback> >,
        decltype ( std::declval<remove_smart_ptr_t<Callback>>()( std::declval<MsgType>()) )//check if the callback has a operator() member with the correct signature
        >
      > : public std::true_type{};

    /*!
     * Checks if a Callable object is of type async_op and if
     * it accepts the given MsgType and returns the expected type
     */
    template< typename Callback, typename MsgType >
    constexpr bool is_future_monad_cb_v = is_future_monad_cb<Callback, MsgType>::value;

    template< typename Callback, typename MsgType, typename = std::void_t<> >
    struct is_sync_monad_cb : public std::false_type{};

    template< typename Callback, typename MsgType >
    struct is_sync_monad_cb<Callback, MsgType
      , std::void_t<
        std::enable_if_t< !is_async_op_v<Callback> >,
        std::enable_if_t< !std::is_same_v< void, decltype ( std::declval<Callback>()(std::declval<MsgType>())) > > > //check if the callback has the correct signature:  cb( MsgType )
      > : public std::true_type{};

    /*!
     * Checks if a Callable object is a syncronous callback type with the expected signature
     */
    template< typename Callback, typename MsgType, typename = std::void_t<> >
    constexpr bool is_sync_monad_cb_v = is_sync_monad_cb<Callback, MsgType>::value;


    template <typename Callback, typename Arg>
    using callback_returns_async_op = is_async_op< std::invoke_result_t<Callback, Arg >>;


    template< typename Callback, typename MsgType, typename = std::void_t<> >
    struct is_sync_monad_cb_with_async_res : public std::false_type{};

    template< typename Callback, typename MsgType >
    struct is_sync_monad_cb_with_async_res<Callback, MsgType
      , std::void_t<
        std::enable_if_t< is_sync_monad_cb<Callback, MsgType>::value >,
        std::enable_if_t< callback_returns_async_op<Callback, MsgType>::value >>
      > : public std::true_type{};

    template< typename Callback, typename MsgType, typename = std::void_t<> >
    struct is_sync_monad_cb_with_sync_res : public std::false_type{};

    template< typename Callback, typename MsgType >
    struct is_sync_monad_cb_with_sync_res<Callback, MsgType
      , std::void_t<
        std::enable_if_t< is_sync_monad_cb<Callback, MsgType>::value >,
        std::enable_if_t< !callback_returns_async_op<Callback, MsgType>::value > >
      > : public std::true_type{};

    /*!
     * Checks if a Callable object is a syncronous callback type with the expected signature
     */
    template <typename Callback, typename Arg>
    constexpr bool is_sync_monad_cb_with_async_res_v = is_sync_monad_cb_with_async_res<Callback, Arg>::value;

    /*!
     * Checks if the given Callback is a function returning a syncronous result
     */
    template <typename Callback, typename Arg>
    constexpr bool is_sync_monad_cb_with_sync_res_v =  is_sync_monad_cb_with_sync_res<Callback, Arg>::value;


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
     * \todo with newer C++ versions revisit the possibility to use non shared ptr types throughout the pipeline instead of
     *       the current smart pointer solution.
     */
    //template <typename Prev, typename AOp, typename Enable = void>
    //struct AsyncResult;

    template <typename T>
    struct is_nested_async : public std::false_type {};

    template <typename T>
    struct is_nested_async<AsyncOpRef<AsyncOpRef<T>>> : public std::true_type {};

    template <typename T>
    constexpr bool is_nested_async_v = is_nested_async<T>::value;


    // case 1: connect async result to async callback
    template <typename PrevRes, typename CallbackOp, typename AOpRes = typename CallbackOp::value_type >
    struct AsyncToAsyncResult : public zyppng::AsyncOp< AOpRes > {

      static_assert( !is_async_op_v<AOpRes>, "A AsyncResult can never return a async value" );
      static_assert( !is_async_op_v<PrevRes>, "A incoming value can never be a async value" );

      AsyncToAsyncResult ( AsyncOpRef<PrevRes> && prevTask, std::shared_ptr<CallbackOp> &&cb )
        : _prevTask( std::move(prevTask) )
        , _myTask( std::move(cb) ) {
        connect();
      }

      AsyncToAsyncResult ( const AsyncToAsyncResult &other ) = delete;
      AsyncToAsyncResult& operator= ( const AsyncToAsyncResult &other ) = delete;

      AsyncToAsyncResult ( AsyncToAsyncResult &&other ) = delete;
      AsyncToAsyncResult& operator= ( AsyncToAsyncResult &&other ) = delete;

      virtual ~AsyncToAsyncResult() {}

      void connect () {
        //not using a lambda here on purpose, binding this into a lambda that is stored in the _prev
        //object causes segfaults on gcc when the lambda is cleaned up with the _prev objects signal instance
        _prevTask->onReady( std::bind( &AsyncToAsyncResult::readyWasCalled, this, std::placeholders::_1) );
        _myTask->onReady( [this] ( AOpRes && res ){
          this->setReady( std::move( res ) );
        });
      }

    private:
      // we need to store the passed argument in our stack, otherwise we
      // run into memory issues if the argument is moved out of the _prevTask object
      // so even though we std::move() the argument further we need to copy it here
      void readyWasCalled ( PrevRes res ) {
        //MIL << "Setting ready: " << typeid(this) << std::endl;
        if ( _prevTask ) {
          //dumpInfo();
          _prevTask.reset();
        }

        _myTask->operator()(std::move(res));
      }

      AsyncOpRef<PrevRes> _prevTask;
      std::shared_ptr<CallbackOp> _myTask;
    };

    template <typename PrevRes, typename Callback, typename Enable = void>
    struct AsyncToSyncResult;

    // case 2: connect async result to sync callback returning a sync value
    template <typename PrevRes, typename Callback>
    struct AsyncToSyncResult<PrevRes, Callback, std::enable_if_t< is_sync_monad_cb_with_sync_res_v<Callback, PrevRes> >>
      : public zyppng::AsyncOp< typename std::invoke_result_t<Callback, PrevRes> > {

      using value_type = std::invoke_result_t<Callback, PrevRes>;
      static_assert( !is_async_op_v<value_type>, "A AsyncResult can never return a async value" );
      static_assert( !is_async_op_v<PrevRes>, "A incoming value can never be a async value" );

      template <typename CBType = Callback>
      AsyncToSyncResult ( AsyncOpRef<PrevRes> && prevTask, CBType &&cb )
        : _prevTask( std::move(prevTask) )
        , _myTask( std::forward<CBType>(cb) ) {
        connect();
      }

      AsyncToSyncResult ( const AsyncToSyncResult &other ) = delete;
      AsyncToSyncResult& operator= ( const AsyncToSyncResult &other ) = delete;

      AsyncToSyncResult ( AsyncToSyncResult &&other ) = delete;
      AsyncToSyncResult& operator= ( AsyncToSyncResult &&other ) = delete;

      virtual ~AsyncToSyncResult() {}

      void connect () {
        //not using a lambda here on purpose, binding this into a lambda that is stored in the _prev
        //object causes segfaults on gcc when the lambda is cleaned up with the _prev objects signal instance
        _prevTask->onReady( std::bind( &AsyncToSyncResult::readyWasCalled, this, std::placeholders::_1) );
      }

    private:
      // we need to store the passed argument in our stack, otherwise we
      // run into memory issues if the argument is moved out of the _prevTask object
      // so even though we std::move() the argument further we need to copy it here
      void readyWasCalled ( PrevRes res ) {
        //MIL << "Setting ready: " << typeid(this) << std::endl;
        if ( _prevTask ) {
          _prevTask.reset();
        }

        this->setReady( std::invoke( _myTask, std::move( res )) );
      }
      AsyncOpRef<PrevRes> _prevTask;
      Callback _myTask;
    };


    // case 3: connect async result to sync callback returning a async value
    template <typename PrevRes, typename Callback>
    struct AsyncToSyncResult<PrevRes, Callback, std::enable_if_t< is_sync_monad_cb_with_async_res_v<Callback, PrevRes> >>
      : public zyppng::AsyncOp< typename remove_smart_ptr_t<std::invoke_result_t<Callback, PrevRes>>::value_type> {

      using value_type = typename remove_smart_ptr_t<std::invoke_result_t<Callback, PrevRes>>::value_type;
      static_assert(!is_async_op_v< value_type >, "A AsyncResult can never return a async value" );

      template <typename CBType = Callback>
      AsyncToSyncResult ( AsyncOpRef<PrevRes> && prevTask, CBType &&cb )
        : _prevTask( std::move(prevTask) )
        , _myTask( std::forward<CBType>(cb) ) {
        connect();
      }

      AsyncToSyncResult ( const AsyncToSyncResult &other ) = delete;
      AsyncToSyncResult& operator= ( const AsyncToSyncResult &other ) = delete;

      AsyncToSyncResult ( AsyncToSyncResult &&other ) = delete;
      AsyncToSyncResult& operator= ( AsyncToSyncResult &&other ) = delete;

      virtual ~AsyncToSyncResult() {}

      void connect () {
        //not using a lambda here on purpose, binding this into a lambda that is stored in the _prev
        //object causes segfaults on gcc when the lambda is cleaned up with the _prev objects signal instance
        _prevTask->onReady( std::bind( &AsyncToSyncResult::readyWasCalled, this, std::placeholders::_1) );
      }

    private:
      // we need to store the passed argument in our stack, otherwise we
      // run into memory issues if the argument is moved out of the _prevTask object
      // so even though we std::move() the argument further we need to copy it here
      void readyWasCalled ( PrevRes res ) {

        //MIL << "Setting ready "<<this<<" step 1: " << typeid(this) << std::endl;
        if ( _prevTask ) {
          _prevTask.reset();
        }

        _asyncResult = std::invoke( _myTask, std::move(res) );
        _asyncResult->onReady( [this]( value_type &&val ) {
          //MIL << "Setting ready "<<this<<" step 2: " << typeid(this) << std::endl;
          this->setReady( std::move(val) );
        });
      }
      AsyncOpRef<PrevRes> _prevTask;
      Callback _myTask;
      AsyncOpRef<value_type> _asyncResult;
    };
  }

  namespace operators {

    template< typename PrevOp , typename Callback,
      std::enable_if_t< detail::is_async_op_v<PrevOp>, int > = 0,
      std::enable_if_t< detail::is_future_monad_cb_v<Callback, typename PrevOp::value_type>, int > = 0 >
    auto operator| ( std::shared_ptr<PrevOp> &&in, std::shared_ptr<Callback>&& c ) -> AsyncOpRef<typename Callback::value_type>
    {
      using PrevOpRes = typename PrevOp::value_type;
      return std::make_shared<detail::AsyncToAsyncResult<PrevOpRes, Callback>>( std::move(in), std::move(c) );
    }

    template< typename PrevOp , typename Callback,
      std::enable_if_t< detail::is_async_op_v<PrevOp>, int > = 0,
      std::enable_if_t< detail::is_sync_monad_cb_with_async_res_v<Callback, typename PrevOp::value_type>, int > = 0
    >
    auto operator| ( std::shared_ptr<PrevOp> &&in, Callback &&c )
    {
      using PrevOpRes = typename PrevOp::value_type;
      using CbType    = std::remove_reference_t<Callback>;
      using Ret       = typename detail::AsyncToSyncResult<PrevOpRes, CbType>::value_type;
      if ( in->isReady() )
        return AsyncOpRef<Ret>( std::invoke( std::forward<Callback>(c), std::move(in->get()) ) );
      return AsyncOpRef<Ret>( new detail::AsyncToSyncResult<PrevOpRes, CbType>( std::move(in), std::forward<Callback>(c) ) );
    }

    template< typename PrevOp , typename Callback,
      std::enable_if_t< detail::is_async_op_v<PrevOp>, int > = 0,
      std::enable_if_t< detail::is_sync_monad_cb_with_sync_res_v<Callback, typename PrevOp::value_type>, int > = 0
    >
    auto operator| ( std::shared_ptr<PrevOp> &&in, Callback &&c )
    {
      using PrevOpRes = typename PrevOp::value_type;
      using CbType    = std::remove_reference_t<Callback>;
      using Ret       = typename detail::AsyncToSyncResult<PrevOpRes, CbType>::value_type;

      if ( in->isReady() )
        return makeReadyResult( std::invoke( std::forward<Callback>(c), std::move(in->get()) ) );
      return AsyncOpRef<Ret>( new detail::AsyncToSyncResult<PrevOpRes, CbType>( std::move(in), std::forward<Callback>(c) ) );
    }

    template< typename PrevRes , typename CallbackOp,
      std::enable_if_t< !detail::is_async_op_v<PrevRes>, int > = 0 ,
      std::enable_if_t< detail::is_future_monad_cb_v<CallbackOp, PrevRes>, int > = 0 >
    auto operator| ( PrevRes &&in, CallbackOp &&c ) -> AsyncOpRef<typename remove_smart_ptr_t<CallbackOp>::value_type>
    {
      // sync message to async callback case
      std::forward<CallbackOp>(c)->operator()( std::forward<PrevRes>(in) );
      return c;
    }

    // sync to sync callback case, we do not need to differentiate between a callback with a async result or a normal one
    // in both cases we simply can use the return type of the callback function
    template< typename SyncRes
      , typename Callback
      , std::enable_if_t< !detail::is_async_op_v< SyncRes >, int > = 0
      , std::enable_if_t< detail::is_sync_monad_cb_v< Callback, SyncRes >, int > = 0
      >
    auto operator| ( SyncRes &&in, Callback &&c )
    {
      return std::forward<Callback>(c)(std::forward<SyncRes>(in));
    }
  }
}

#endif
