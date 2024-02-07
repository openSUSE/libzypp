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
#ifndef ZYPPNG_PIPELINES_ALGORITHM_H_INCLUDED
#define ZYPPNG_PIPELINES_ALGORITHM_H_INCLUDED

#include <zypp-core/zyppng/async/AsyncOp>
#include <functional>

namespace zyppng {

  namespace detail {

    struct ContinueUntilValidPredicate {
      template< class Arg >
      bool operator()( const Arg &value ) {
        // works if operator bool() const is implemented by type Arg
        return !value;
      }
    };

    template <typename T>
    struct showme;

    template < class Container, class AsyncResType, class Transformation, class Predicate, class DefaultType >
    struct AsyncFirstOfImpl : public AsyncOp<AsyncResType> {

      AsyncFirstOfImpl( Container &&inData, Transformation &&transFunc, DefaultType &&defaultVal, Predicate &&predicate  )
        : _inData    ( std::move(inData) )
        , _transFunc ( std::move(transFunc) )
        , _defaultVal( std::move(defaultVal) )
        , _predicate ( std::move(predicate))
        , _currIter  ( _inData.begin() ) { execute(); }

    private:
      void execute() {
        // seems this is empty we are ready
        if ( _currIter == _inData.end() ) {
          return returnDefault();
        }
        invokeCurrent();
      }

      void resultReady ( AsyncResType &&res ) {

        if ( !_predicate(res) ) {
          _currIter = std::next( _currIter );
          // seems we reached the end
          if ( _currIter == _inData.end() ) {
            return returnDefault();
          }
          invokeCurrent();
        } else {
          this->setReady ( std::move(res) );
          return;
        }

      }

      void invokeCurrent() {
        _currentPipeline = std::invoke( _transFunc, std::move( *_currIter ) );
        _currentPipeline->onReady( [ this ](  AsyncResType &&res  )  {
          this->resultReady( std::move(res));
        });
      }
      void returnDefault() {
        this->setReady ( std::move(_defaultVal) );
        return;
      }

    private:
      Container _inData;
      Transformation _transFunc;
      DefaultType _defaultVal;
      Predicate _predicate;
      typename Container::iterator _currIter;
      AsyncOpRef<AsyncResType> _currentPipeline;
    };

    template < class Transformation, class Predicate, class DefaultType >
    struct FirstOfHelper {

      FirstOfHelper( Transformation transFunc, DefaultType defaultVal, Predicate predicate  )
        : _transFunc ( std::move(transFunc) )
        , _defaultVal( std::move(defaultVal) )
        , _predicate ( std::move(predicate)) { }

      template <  class Container
                , typename ...CArgs>
      auto operator()( Container &&container ) {

        using InputType  = typename Container::value_type;
        static_assert( std::is_invocable_v<Transformation, InputType>, "Transformation function must take the container value type as input " );
        static_assert( std::is_rvalue_reference_v<decltype(std::forward<Container>(container))>, "Input container must be a rvalue reference" );

        using OutputType = std::invoke_result_t<Transformation, InputType>;

        if constexpr ( detail::is_async_op_v<OutputType> ) {

          using AsyncResultType = typename remove_smart_ptr_t<OutputType>::value_type;

          static_assert( std::is_same_v<AsyncResultType, DefaultType>, "Default type and transformation result type must match" );

          using ImplType = AsyncFirstOfImpl<Container, AsyncResultType,Transformation, Predicate, DefaultType>;
          return static_cast<AsyncOpRef<AsyncResultType>>( std::make_shared<ImplType>( std::forward<Container>(container), std::move(_transFunc), std::move(_defaultVal), std::move(_predicate) ) );

        } else {

          static_assert( std::is_same_v<OutputType, DefaultType>, "Default type and transformation result type must match" );

          for ( auto &in : std::forward<Container>(container) ) {
            OutputType res = std::invoke( _transFunc, std::move(in) );
            if ( !_predicate(res) ) {
              return res;
            }
          }
          return _defaultVal;
        }
      }

    private:
      Transformation _transFunc;
      DefaultType _defaultVal;
      Predicate _predicate;
    };

  }

  /*!
   * Provide a dummy error type for when there is no entry found
   */
  class NotFoundException : public zypp::Exception {
  public:
    NotFoundException() : zypp::Exception("No Entry found"){}
  };

  template < class Transformation, class DefaultType, class Predicate >
  inline auto firstOf( Transformation &&transformFunc, DefaultType &&def, Predicate &&predicate = detail::ContinueUntilValidPredicate() ) {
    return detail::FirstOfHelper<Transformation, Predicate, DefaultType> ( std::forward<Transformation>(transformFunc), std::forward<DefaultType>(def), std::forward<Predicate>(predicate) );
  }

  namespace detail {

    template <typename Excpt, typename ...Rest>
    bool containsOneOfExceptionImpl( const std::exception_ptr exceptionPtr ) {
      try {
        if constexpr ( sizeof...(Rest) == 0  ) {
          // on the lowest level we throw the exception
          std::rethrow_exception ( exceptionPtr );
        } else {
          return  containsOneOfExceptionImpl<Rest...>(exceptionPtr);
        }
      } catch ( const Excpt &e ) {
        return true;
      }
    }

  }

  /*!
   * Checks if a given std::exception_ptr contains one of the given exception types
   * in the template arguments:
   *
   * \code
   *
   * expected<int> result = someFuncReturningExpected();
   * if ( !result && containsOneOfException<zypp::FileNotFoundException, SomeOtherException>() ) {
   *  std::cout << "Contains either zypp::FileNotFoundException or SomeOtherException" << std::endl;
   * }
   *
   * \endcode
   *
   * Detects exceptions by throwing and catching them, so always pass the specific exception types to check for..
   */
  template <typename ...Excpt>
  bool containsOneOfException( const std::exception_ptr exceptionPtr ) {
    try {
      return detail::containsOneOfExceptionImpl<Excpt...>( exceptionPtr );
    } catch ( ... ) {}
    return false;
  }

  /*!
   * Checks if a given std::exception_ptr contains the given exception type
   * in the template type argument:
   *
   * \code
   *
   * expected<int> result = someFuncReturningExpected();
   * if ( !result && containsException<zypp::FileNotFoundException>() ) {
   *  std::cout << "Contains zypp::FileNotFoundException" << std::endl;
   * }
   *
   * \endcode
   */
  template <typename Excpt>
  bool containsException( const std::exception_ptr exceptionPtr ) {
    try {
      std::rethrow_exception ( exceptionPtr );;
    } catch ( const Excpt &e ) {
      return true;
    } catch ( ... ) {}
    return false;
  }


}




#endif
