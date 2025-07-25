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
* Based on code by Ivan Čukić (BSD/MIT licensed) from the functional cpp book
*/

#ifndef ZYPP_ZYPPNG_MONADIC_EXPECTED_H
#define ZYPP_ZYPPNG_MONADIC_EXPECTED_H

#include <zypp-core/ng/meta/Functional>
#include <zypp-core/ng/pipelines/AsyncResult>
#include <zypp-core/ng/pipelines/Wait>
#include <zypp-core/ng/pipelines/Transform>

namespace zyppng {

  template<typename T, typename E = std::exception_ptr>
  class ZYPP_NODISCARD expected {
  protected:
      union {
          T m_value;
          E m_error;
      };

      bool m_isValid;

      expected() // used internally
      {
      }

  public:

    using value_type = T;
    using error_type = E;

      ~expected()
      {
          if (m_isValid) {
              m_value.~T();
          } else {
              m_error.~E();
          }
      }

      expected(const expected &other)
          : m_isValid(other.m_isValid)
      {
          if (m_isValid) {
              new (&m_value) T(other.m_value);
          } else {
              new (&m_error) E(other.m_error);
          }
      }

      expected(expected &&other) noexcept
          : m_isValid(other.m_isValid)
      {
          if (m_isValid) {
              new (&m_value) T( std::move(other.m_value) );
          } else {
              new (&m_error) E( std::move(other.m_error) );
          }
      }

      expected &operator= (expected other)
      {
          swap(other);
          return *this;
      }

      void swap(expected &other) noexcept
      {
          using std::swap;
          if (m_isValid) {
              if (other.m_isValid) {
                  // Both are valid, just swap the values
                  swap(m_value, other.m_value);

              } else {
                  // We are valid, but the other one is not
                  // we need to do the whole dance
                  auto temp = std::move(other.m_error);       // moving the error into the temp
                  other.m_error.~E();                         // destroying the original error object
                  new (&other.m_value) T(std::move(m_value)); // moving our value into the other
                  m_value.~T();                               // destroying our value object
                  new (&m_error) E(std::move(temp));          // moving the error saved to the temp into us
                  std::swap(m_isValid, other.m_isValid);      // swap the isValid flags
              }

          } else {
              if (other.m_isValid) {
                  // We are not valid, but the other one is,
                  // just call swap on other and rely on the
                  // implementation in the previous case
                  other.swap(*this);

              } else {
                  // Everything is rotten, just swap the errors
                  swap(m_error, other.m_error);
                  std::swap(m_isValid, other.m_isValid);
              }
          }
      }

      template <typename... ConsParams>
      static expected success(ConsParams && ...params)
      {
          // silence clang-tidy about uninitialized class members, we manually intialize them.
          // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.UninitializedObject)
          expected result;
          result.m_isValid = true;
          new(&result.m_value) T(std::forward<ConsParams>(params)...);
          return result;
      }

      template <typename... ConsParams>
      static expected error(ConsParams && ...params)
      {
          // silence clang-tidy about uninitialized class members, we manually intialize them.
          // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.UninitializedObject)
          expected result;
          result.m_isValid = false;
          new(&result.m_error) E(std::forward<ConsParams>(params)...);
          return result;
      }

      operator bool() const
      {
          return m_isValid;
      }

      bool is_valid() const
      {
          return m_isValid;
      }

      #ifdef NO_EXCEPTIONS
      #    define THROW_MSG_IF_EXCEPTIONS_ARE_ENABLED(WHAT) std::terminate()
      #else
      #    define THROW_MSG_IF_EXCEPTIONS_ARE_ENABLED(WHAT) throw std::logic_error(WHAT)
      #endif

      T &get()
      {
          if (!m_isValid) THROW_MSG_IF_EXCEPTIONS_ARE_ENABLED("expected<T, E> contains no value");
          return m_value;
      }

      const T &get() const
      {
          if (!m_isValid) THROW_MSG_IF_EXCEPTIONS_ARE_ENABLED("expected<T, E> contains no value");
          return m_value;
      }

      /*!
       * Unwraps the value if the expected is valid, otherwise throws an exception.
       * If the Error type is std::exception_ptr the exception will be rethrown, otherwise
       * the Error object is directly thrown as if calling:
       * \code
       * expected<int,MyError> test = functionReturningError();
       * throw test.error();
       * \endcode
       */
      T &unwrap()
      {
          if (!m_isValid) {
#ifdef NO_EXCEPTIONS
            std::terminate();
#else
            if constexpr ( std::is_same_v<E, std::exception_ptr> ) {
              std::rethrow_exception ( error() );
            } else {
              throw error();
            }
#endif
          }
          return m_value;
      }

      const T &unwrap() const
      {
        if (!m_isValid) {
#ifdef NO_EXCEPTIONS
          std::terminate();
#else
          if constexpr ( std::is_same_v<E, std::exception_ptr>() ) {
            std::rethrow_exception ( error() );
          } else {
            throw error();
          }
#endif
        }
          return m_value;
      }

      T &operator* ()
      {
          return get();
      }

      const T &operator* () const
      {
          return get();
      }

      T *operator-> ()
      {
          return &get();
      }

      const T *operator-> () const
      {
          return &get();
      }

      E &error()
      {
          if (m_isValid) THROW_MSG_IF_EXCEPTIONS_ARE_ENABLED("There is no error in this expected<T, E>");
          return m_error;
      }

      const E &error() const
      {
          if (m_isValid) THROW_MSG_IF_EXCEPTIONS_ARE_ENABLED("There is no error in this expected<T, E>");
          return m_error;
      }

      #undef THROW_IF_EXCEPTIONS_ARE_ENABLED

      template <typename F>
      void visit(F f) {
          if (m_isValid) {
              f(m_value);
          } else {
              f(m_error);
          }
      }
  };


  template<typename E>
  class ZYPP_NODISCARD expected<void, E> {
  private:
      union {
          void* m_value;
          E m_error;
      };

      bool m_isValid;

      expected() {} //used internally

  public:
      ~expected()
      {
          if (m_isValid) {
              // m_value.~T();
          } else {
              m_error.~E();
          }
      }

      expected(const expected &other)
          : m_isValid(other.m_isValid)
      {
          if (m_isValid) {
              // new (&m_value) T(other.m_value);
          } else {
              new (&m_error) E(other.m_error);
          }
      }

      expected(expected &&other) noexcept
          : m_isValid(other.m_isValid)
      {
          if (m_isValid) {
              // new (&m_value) T(std::move(other.m_value));
          } else {
              new (&m_error) E(std::move(other.m_error));
          }
      }

      expected &operator= (expected other)
      {
          swap(other);
          return *this;
      }

      void swap(expected &other) noexcept
      {
          using std::swap;
          if (m_isValid) {
              if (other.m_isValid) {
                  // Both are valid, we do not have any values
                  // to swap

              } else {
                  // We are valid, but the other one is not.
                  // We need to move the error into us
                  auto temp = std::move(other.m_error);    // moving the error into the temp
                  other.m_error.~E();                      // destroying the original error object
                  new (&m_error) E(std::move(temp));       // moving the error into us
                  std::swap(m_isValid, other.m_isValid);   // swapping the isValid flags
              }

          } else {
              if (other.m_isValid) {
                  // We are not valid, but the other one is,
                  // just call swap on other and rely on the
                  // implementation in the previous case
                  other.swap(*this);

              } else {
                  // Everything is rotten, just swap the errors
                  swap(m_error, other.m_error);
                  std::swap(m_isValid, other.m_isValid);
              }
          }
      }

      static expected success()
      {
        // silence clang-tidy about uninitialized class members, we manually intialize them.
        // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.UninitializedObject)
        expected result;
        result.m_isValid = true;
        result.m_value = nullptr;
        return result;
      }

      template <typename... ConsParams>
      static expected error(ConsParams && ...params)
      {
        // silence clang-tidy about uninitialized class members, we manually intialize them.
        // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.UninitializedObject)
        expected result;
        result.m_isValid = false;
        new(&result.m_error) E(std::forward<ConsParams>(params)...);
        return result;
      }

      operator bool() const
      {
          return m_isValid;
      }

      bool is_valid() const
      {
          return m_isValid;
      };

      #ifdef NO_EXCEPTIONS
      #    define THROW_IF_EXCEPTIONS_ARE_ENABLED(WHAT) std::terminate()
      #else
      #    define THROW_IF_EXCEPTIONS_ARE_ENABLED(WHAT) throw std::logic_error(WHAT)
      #endif

      E &error()
      {
          if (m_isValid) THROW_IF_EXCEPTIONS_ARE_ENABLED("There is no error in this expected<T, E>");
          return m_error;
      }

      const E &error() const
      {
          if (m_isValid) THROW_IF_EXCEPTIONS_ARE_ENABLED("There is no error in this expected<T, E>");
          return m_error;
      }

      void unwrap() const
      {
          if (!m_isValid) {
#ifdef NO_EXCEPTIONS
            std::terminate();
#else
             if constexpr ( std::is_same_v<E, std::exception_ptr> ) {
              std::rethrow_exception ( error() );
            } else {
              throw error();
            }
#endif
          }
      }

  };

  template <typename Type, typename Err = std::exception_ptr >
  static expected<std::decay_t<Type>,Err> make_expected_success( Type &&t )
  {
    return expected<std::decay_t<Type>,Err>::success( std::forward<Type>(t) );
  }

  namespace detail {

    // helper to figure out the return type for a mbind callback, if the ArgType is void the callback is considered to take no argument.
    // Due to how std::conditional works, we cannot pass std::invoke_result_t but instead use the template type std::invoke_result, since
    // one of the two options have no "::type" because the substitution fails, this breaks the std::conditional_t since it can only work with two well formed
    // types. Instead we pass in the template types and evaluate the ::type in the end, when the correct invoke_result was chosen.
    template < typename Function, typename ArgType>
    using mbind_cb_result_t = typename std::conditional_t< std::is_same_v<ArgType,void>, std::invoke_result<Function>,std::invoke_result<Function, ArgType> >::type;

    template <typename T>
    bool waitForCanContinueExpected( const expected<T> &value ) {
      return value.is_valid();
    }
  }


  template < typename T
           , typename E
           , typename Function
           , typename ResultType = detail::mbind_cb_result_t<Function, T>
           >
  ResultType and_then( const expected<T, E>& exp, Function &&f)
  {
      if (exp) {
        if constexpr ( std::is_same_v<T,void> )
          return std::invoke( std::forward<Function>(f) );
        else
          return std::invoke( std::forward<Function>(f), exp.get() );
      } else {
        if constexpr ( !detail::is_async_op< remove_smart_ptr_t<ResultType> >::value )
          return ResultType::error(exp.error());
        else
          return makeReadyResult( remove_smart_ptr_t<ResultType>::value_type::error(exp.error()) );
      }
  }

  template < typename T
    , typename E
    , typename Function
    , typename ResultType = detail::mbind_cb_result_t<Function, T>
    >
  ResultType and_then( expected<T, E> &&exp, Function &&f)
  {
    if (exp) {
      if constexpr ( std::is_same_v<T,void> )
        return std::invoke( std::forward<Function>(f) );
      else
        return std::invoke( std::forward<Function>(f), std::move(exp.get()) );
    } else {
      if constexpr ( !detail::is_async_op< ResultType >::value )
        return ResultType::error( std::move(exp.error()) );
      else
        return makeReadyResult( remove_smart_ptr_t<ResultType>::value_type::error(exp.error()) );
    }
  }

  template < typename T
    , typename E
    , typename Function
    , typename ResultType = detail::mbind_cb_result_t<Function, E>
    >
  ResultType or_else( const expected<T, E>& exp, Function &&f)
  {
    if (!exp) {
      return std::invoke( std::forward<Function>(f), exp.error() );
    } else {
      if constexpr ( !detail::is_async_op< remove_smart_ptr_t<ResultType> >::value )
        return exp;
      else
        return makeReadyResult( std::move(exp) );
    }
  }

  template < typename T
    , typename E
    , typename Function
    , typename ResultType = detail::mbind_cb_result_t<Function, E>
    >
  ResultType or_else( expected<T, E>&& exp, Function &&f)
  {
    if (!exp) {
      return std::invoke( std::forward<Function>(f), std::move(exp.error()) );
    } else {
      if constexpr ( !detail::is_async_op< remove_smart_ptr_t<ResultType> >::value )
        return exp;
      else
        return makeReadyResult( std::move(exp) );
    }
  }


  /*!
   * Collects all values from a Container of \ref expected values, returning the
   * contained values or the first error encountered as a expected<Container<T>>
   */
  template < template< class, class... > class Container,
    typename T,
    typename E,
    typename ...CArgs >
  std::enable_if_t<!std::is_same_v<void, T>, expected<Container<T>,E>> collect( Container<expected<T, E>, CArgs...>&& in ) {
    Container<T> res;
    for( auto &v : in ) {
      if ( !v )
        return expected<Container<T>,E>::error( std::move(v.error()) );
      res.push_back( std::move(v.get()) );
    }
    return expected<Container<T>,E>::success( std::move(res) );
  }

  /*!
   * Specialization of collect working on a Container of expected<void> values,
   * returning either success or the error encountered.
   */
  template < template< class, class... > class Container,
    typename T,
    typename E,
    typename ...CArgs >
  std::enable_if_t<std::is_same_v<void, T>, expected<T, E>> collect( Container<expected<T, E>, CArgs...>&& in ) {
    for( auto &v : in ) {
      if ( !v )
        return expected<T,E>::error( std::move(v.error()) );
    }
    return expected<T,E>::success( );
  }

  template < typename T
    , typename E
    , typename Function
    >
  expected<T, E> inspect( expected<T, E> exp, Function &&f )
  {
    if (exp) {
      const auto &val = exp.get();
      std::invoke( std::forward<Function>(f), val );
    }
    return exp;
  }

  template < typename T
    , typename E
    , typename Function
    >
  expected<T, E> inspect_err( expected<T, E> exp, Function &&f)
  {
    if (!exp) {
      const auto &err = exp.error();
      std::invoke( std::forward<Function>(f), err );
    }
    return exp;
  }


  namespace detail {

    template <typename Callback>
    struct and_then_helper {
      Callback function;

      template< typename T, typename E >
      auto operator()( const expected<T, E>& exp ) {
        return and_then( exp, function );
      }

      template< typename T, typename E >
      auto operator()( expected<T, E>&& exp ) {
        return and_then( std::move(exp), function );
      }
    };

    template <typename Callback>
    struct or_else_helper {
      Callback function;

      template< typename T, typename E >
      auto operator()( const expected<T, E>& exp ) {
        return or_else( exp, function );
      }

      template< typename T, typename E >
      auto operator()( expected<T, E>&& exp ) {
        return or_else( std::move(exp), function );
      }
    };

    template <typename Callback>
    struct inspect_helper {
      Callback function;

      template< typename T, typename E >
      auto operator()( expected<T, E>&& exp ) {
        return inspect( std::move(exp), function );
      }
    };

    template <typename Callback>
    struct inspect_err_helper {
      Callback function;

      template< typename T, typename E >
      auto operator()( expected<T, E>&& exp ) {
        return inspect_err( std::move(exp), function );
      }
    };

    struct collect_helper {
      template < typename T >
      inline auto operator()( T&& in ) {
        return collect( std::forward<T>(in) );
      }
    };
  }

  namespace operators {
    template <typename Fun>
    auto mbind ( Fun && function ) {
      return detail::and_then_helper<Fun> {
        std::forward<Fun>(function)
      };
    }

    template <typename Fun>
    auto and_then ( Fun && function ) {
      return detail::and_then_helper<Fun> {
        std::forward<Fun>(function)
      };
    }

    template <typename Fun>
    auto or_else ( Fun && function ) {
      return detail::or_else_helper<Fun> {
        std::forward<Fun>(function)
      };
    }

    template <typename Fun>
    auto inspect ( Fun && function ) {
      return detail::inspect_helper<Fun> {
        std::forward<Fun>(function)
      };
    }

    template <typename Fun>
    auto inspect_err ( Fun && function ) {
      return detail::inspect_err_helper<Fun> {
        std::forward<Fun>(function)
      };
    }

    inline detail::collect_helper collect() {
      return detail::collect_helper();
    }
  }


  /*!
   * This is logically the same as \code transform() | collect() \endcode , but the inner loop will stop waiting / transforming
   * as soon as a error is encountered and returns the error right away.
   */
  template < template< class, class... > class Container,
    typename Msg,
    typename Transformation,
    typename Ret = std::result_of_t<Transformation(Msg)>,
    typename ...CArgs
    >
  auto transform_collect( Container<Msg, CArgs...>&& in, Transformation &&f )
  {
    using namespace zyppng::operators;
    if constexpr ( detail::is_async_op_v<Ret> ) {
      using AsyncRet = typename remove_smart_ptr_t<Ret>::value_type;
      static_assert( is_instance_of<expected, AsyncRet>::value, "Transformation function must return a expected type" );

      return transform( std::move(in), f )
             // cancel WaitFor if one of the async ops returns a error
             | detail::WaitForHelperExt<AsyncRet>( detail::waitForCanContinueExpected<typename AsyncRet::value_type> )
             | collect();

    } else {
      static_assert( is_instance_of<expected, Ret>::value, "Transformation function must return a expected type" );
      Container<typename Ret::value_type> results;
      for ( auto &v : in ) {
        auto res = f(std::move(v));
        if ( res ) {
          results.push_back( std::move(res.get()) );
        } else {
          return expected<Container<typename Ret::value_type>>::error( res.error() );
        }
      }
      return expected<Container<typename Ret::value_type>>::success( std::move(results) );
    }
  }

  namespace detail {
    template <typename Fun>
    struct transform_collect_helper {
      Fun _callback;
      template <typename T>
      auto operator() ( T &&in ) {
        return transform_collect( std::forward<T>(in), _callback );
      }
    };
  }

  namespace operators {
    template <typename Transformation>
    auto transform_collect( Transformation &&f ) {
      return detail::transform_collect_helper{ std::forward<Transformation>(f)};
    }
  }



}

#endif

