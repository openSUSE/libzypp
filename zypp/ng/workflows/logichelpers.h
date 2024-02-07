/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_LOGICHELPERS_INCLUDED
#define ZYPP_NG_LOGICHELPERS_INCLUDED

#include <zypp-core/zyppng/async/AsyncOp>

namespace zyppng
{

  namespace detail {
    template <typename Op, typename = void>
    struct LogicBaseExec : public Op { };

    template <typename Op>
    struct LogicBaseExec<Op, std::enable_if_t<detail::is_async_op_v<Op>>> : public Op
    {
    protected:
      AsyncOpRef<typename Op::value_type> _innerPipeline;
    };

  }

  /*!
   * Logic base class template, this allows us to implement certain workflows
   * in a sync/async agnostic way, based on the OpType the code is either
   * sync or async:
   *
   * \code
   *
   *  // ------------------ computeint.h ---------------------------
   *
   *  // the public interface for the workflows are just functions, usually with some
   *  // context object just save passing a gazillion of arguments to the func and
   *  // to carry some state.
   *  AsyncOpRef<expected<int>> computeInt( ContextRef context );
   *  expected<int> computeInt( SyncContextRef context );
   *
   *  // ------------------ computeint.cc ---------------------------
   *
   *  namespace {
   *  template <
   *      typename Executor ///< The Final Executor type that subclasses from ComputeIntLogic
   *    , typename OpType   ///< The SyncOp<T> or AsyncOp<T> result type, LogicBase will derive from it
   *  >
   *  struct ComputeIntLogic : public LogicBase<Executor, OpType>
   *  {
   *    // declare some helper tools from the base
   *    ZYPP_ENABLE_LOGIC_BASE(Executor, OpType);
   *
   *    // declare our context type based on the OpType we are
   *    using ZyppContextRefType = MaybeAsyncContextRef<OpType>;
   *
   *    // all arguments to the sync/async function are passed to the constructor
   *    LogicImpl( ZyppContextRefType ctx )
   *    : LogicBase<Executor>()
   *    , _ctx( std::move(ctx) )
   *    {}
   *
   *    // every logic type must implement execute() without any arguments, returning
   *    // the result type. In async world the pipeline will be stored in the object until its ready
   *    MaybeAsyncRef<<expected<int>> execute() {
   *      return executor()->computeSomeInt()
   *      | and_then( [this]( int result ) {
   *        std::cout<< "Int calculated as: " << result << std::endl;
   *        return expected<int>::success(result);
   *      });
   *    }
   *
   *    protected:
   *    ZyppContextRefType _ctx;
   *  };
   *
   *  // AsyncOp Executor for ComputeIntLogic implementation,
   *  // here we put all code that can not be implementedin a
   *  // sync/async agnostic way. Usually sending reports to the user.
   *  // If no such code is required, simply use the \ref SimpleExecutor template
   *  class ComputeIntAsyncExecutor : public ComputeIntLogic<ComputeIntAsyncExecutor, AsyncOp<int>>
   *  {
   *    // usually we just want to use Logic's constructors
   *    using ComputeIntLogic<ComputeIntAsyncExecutor, AsyncOp<int>>::ComputeIntLogic;
   *    AsyncOpRef<expected<int>> computeSomeInt() {
   *      // .... some implementation that asynchronously computes a int and returns it
   *    }
   *  };
   *
   *  // SyncOp Executor for ComputeIntLogic implementation,
   *  // here we put all the sync variants of code that can not be implemented in a
   *  // sync/async agnostic way. Usually sending reports to the user.
   *  // If no such code is required, simply use the \ref SimpleExecutor template
   *  class ComputeIntSyncExecutor : public ComputeIntLogic<ComputeIntAsyncExecutor, SyncOp<int>>
   *  {
   *    using ComputeIntLogic<ComputeIntAsyncExecutor, SyncOp<int>>::ComputeIntLogic;
   *    expected<int> computeSomeInt(){
   *      // .... some implementation that synchronously computes a int and returns it
   *    }
   *  };
   *
   *  } //namespace
   *
   *  // by overloading this function on the (Sync)ContextRef type other workflow implementations
   *  // can call the async/sync version just by passing the right parameters
   *  AsyncOpRef<expected<int>> computeInt( ContextRef context ) {
   *    // the run() static method is provided by the LogicBase subclass
   *    return ComputeIntAsyncExecutor::run( std::move(context) );
   *  }
   *
   *  expected<int> computeInt( SyncContextRef context ) {
   *    // the run() static method is provided by the LogicBase subclass
   *    return ComputeIntSyncExecutor::run( std::move(context) );
   *  }
   *
   * \endcode
   */
  template <typename Executor, typename OpType>
  struct LogicBase : public detail::LogicBaseExec<OpType> {

    using ExecutorType = Executor;
    using Result = typename OpType::value_type;

    /*!
     * Evaluates to either \a AsyncOpRef<Type> or \a Type , based on the Executor type.
     */
    template<class Type>
    using MaybeAsyncRef = std::conditional_t< detail::is_async_op_v<OpType>, AsyncOpRef<Type>, Type>;

    LogicBase( ){ }
    virtual ~LogicBase(){}

    template <typename ...Args, typename FOpType = OpType>
    static std::enable_if_t< detail::is_async_op_v<FOpType>, AsyncOpRef<Result>> run( Args &&...args ) {
      auto op = std::make_shared<Executor>( std::forward<Args>(args)... );
      op->asyncExecute();
      return op;
    }

    template <typename ...Args, typename FOpType = OpType>
    static std::enable_if_t< !detail::is_async_op_v<FOpType>, Result> run(  Args &&...args ) {
      return Executor( std::forward<Args>(args)... ).execute();
    }


    /*!
     * Allows access to functionality provided by the \a Executor parent class,
     * usually used for sending user reports because those are too different for sync and async.
     */
    Executor *executor () {
      return static_cast<Executor *>(this);
    }

    /*!
     * Returns the value given in \a res either as \ref AsyncOpRef<T> or as T depending on the
     * \a Executor type.
     */
    template <typename T>
    inline auto makeReadyResult( T &&res ) {
      return zyppng::makeReadyResult<T, zyppng::detail::is_async_op_v<OpType>>  ( std::forward<T>(res) );
    }

  private:
    template <typename FOpType = OpType>
    std::enable_if_t< detail::is_async_op_v<FOpType>, void> asyncExecute() {
      this->_innerPipeline = static_cast<Executor*>(this)->execute();
      this->_innerPipeline->onReady([this]( auto &&val ){
        this->setReady( std::forward<decltype(val)>(val) );
      });
    }
  };

  /*!
   * Helper base class for the sync
   * only parts of a logic implementation
   */
  template <typename Result>
  struct SyncOp : public Base {
    using value_type = Result;
  };


  template <template<typename, typename> typename Logic , typename OpType>
  struct SimpleExecutor : public Logic<SimpleExecutor<Logic, OpType>, OpType>
  {
  public:
    template <typename ...Args>
    SimpleExecutor( Args &&...args ) : Logic<SimpleExecutor<Logic, OpType>, OpType>( std::forward<Args>(args)...) {}
  };


  /*!
   * Forward declares some convenience functions from Base class so they can be used directly
   * See \ref LogicBase for documentation
   */
  #define ZYPP_ENABLE_LOGIC_BASE(Executor, OpType) \
    using LogicBase<Executor, OpType>::executor; \
    using LogicBase<Executor, OpType>::makeReadyResult; \
    template<class T> \
    using MaybeAsyncRef = typename LogicBase<Executor, OpType>:: template MaybeAsyncRef<T>


  template <typename Sub>
  struct LogicHelper  {

    template<class Type>
    using MaybeAsyncRef = std::conditional_t< detail::is_async_op_v<Sub>, AsyncOpRef<Type>, Type>;

    virtual ~LogicHelper(){}

    inline Sub *subClass () {
      return static_cast<Sub *>(this);
    }

    template <typename T>
    inline auto makeReadyResult( T &&res ) {
      return zyppng::makeReadyResult<T, zyppng::detail::is_async_op_v<Sub>>  ( std::forward<T>(res) );
    }
  };

#define ZYPP_ENABLE_LOGIC_HELPER(Sub) \
  using LogicHelper<Sub>::subClass; \
  using LogicHelper<Sub>::makeReadyResult; \
  template<class T> \
  using MaybeAsyncRef = typename LogicHelper<Sub>:: template MaybeAsyncRef<T>


}


#endif
