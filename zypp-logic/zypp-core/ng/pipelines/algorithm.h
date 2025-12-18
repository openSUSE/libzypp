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

#include <functional>
#include <zypp-core/base/Exception.h>
#include <zypp-core/ng/pipelines/operators.h>

namespace zyppng {

  namespace detail {

    struct ContinueUntilValidPredicate {
      template< class Arg >
      bool operator()( const Arg &value ) {
        // works if operator bool() const is implemented by type Arg
        if ( value ) return true;
        return false;
      }
    };

    template <typename T>
    struct showme;

    template < class Container, class Transformation, class Predicate, class DefaultType, typename sfinae = void >
    struct FirstOfImpl;

    template < class Container, class Transformation, class Predicate, class DefaultType, typename sfinae >
    struct FirstOfImpl
    {
        using InputType  = typename Container::value_type;
        static_assert( std::is_invocable_v<Transformation, InputType>, "Transformation function must take the container value type as input " );

        using OutputType = std::invoke_result_t<Transformation, InputType>;

        template <typename C = Container>
        static auto execute ( C &&container, Transformation transFunc, DefaultType defaultVal, Predicate predicate) {
          static_assert( std::is_same_v<OutputType, DefaultType>, "Default type and transformation result type must match" );

          for ( auto &in : std::forward<C>(container) ) {
            OutputType res = std::invoke( transFunc, std::move(in) );
            if ( predicate(res) ) {
              return res;
            }
          }
          return defaultVal;
        }
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
        static_assert( std::is_rvalue_reference_v<decltype(std::forward<Container>(container))>, "Input container must be a rvalue reference" );
        return FirstOfImpl<Container, Transformation, Predicate, DefaultType>::execute( std::forward<Container>(container), std::move(_transFunc), std::move(_defaultVal), std::move(_predicate) );
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
    bool containsOneOfExceptionImpl( const std::exception_ptr &exceptionPtr ) {
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
   * if ( !result && containsOneOfException<zypp::FileNotFoundException, SomeOtherException>( result.error() ) ) {
   *  std::cout << "Contains either zypp::FileNotFoundException or SomeOtherException" << std::endl;
   * }
   *
   * \endcode
   *
   * Detects exceptions by throwing and catching them, so always pass the specific exception types to check for..
   */
  template <typename ...Excpt>
  bool containsOneOfException( const std::exception_ptr &exceptionPtr ) {
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
   * if ( !result && containsException<zypp::FileNotFoundException>( result.error() ) ) {
   *  std::cout << "Contains zypp::FileNotFoundException" << std::endl;
   * }
   *
   * \endcode
   */
  template <typename Excpt>
  bool containsException( const std::exception_ptr &exceptionPtr ) {
    try {
      std::rethrow_exception ( exceptionPtr );;
    } catch ( const Excpt &e ) {
      return true;
    } catch ( ... ) {}
    return false;
  }


}

#ifdef ZYPP_ENABLE_ASYNC
#include <zypp-core/ng/async/pipelines/algorithm.hpp>
#endif

#endif
