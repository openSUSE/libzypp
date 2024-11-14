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

#ifndef ZYPP_ZYPPNG_PIPELINES_OPERATORS_H
#define ZYPP_ZYPPNG_PIPELINES_OPERATORS_H

#include <utility>

namespace zyppng {

  namespace detail {
    template <typename Callback>
    struct and_then_helper {
      Callback function;

      template< typename T >
      auto operator()( T&& exp ) {
        return and_then( std::forward<T>(exp), function );
      }
    };

    template <typename Callback>
    struct or_else_helper {
      Callback function;

      template< typename T >
      auto operator()(T&& exp ) {
        return or_else( std::forward<T>(exp), function );
      }
    };

    template <typename Callback>
    struct inspect_helper {
      Callback function;

      template< typename T>
      auto operator()( T &&exp ) {
        return inspect( std::forward<T>(exp), function );
      }
    };

    template <typename Callback>
    struct inspect_err_helper {
      Callback function;

      template< typename T>
      auto operator()( T &&exp ) {
        return inspect_err( std::forward<T>(exp), function );
      }
    };
  }


  namespace operators {
    /*!
     * \brief inspect
     * Invokes the callback if the value passed to the inspect implementation
     * evaluates as valid, e.g for a \ref expected this means that it does not contain an error.
     */
    template <typename Fun>
    auto inspect ( Fun && function ) {
      return detail::inspect_helper<Fun> {
        std::forward<Fun>(function)
      };
    }

    /*!
     * \brief inspect_err
     * Invokes the callback if the value passed to the inspect implementation
     * evaluates as invalid, e.g for a \ref expected this means that it does contain an error.
     */
    template <typename Fun>
    auto inspect_err ( Fun && function ) {
      return detail::inspect_err_helper<Fun> {
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
  }

}

#endif
