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

#ifndef ZYPP_ZYPPNG_PIPELINES_OPTIONAL_H
#define ZYPP_ZYPPNG_PIPELINES_OPTIONAL_H

#include <optional>

#ifdef __cpp_lib_optional
#include <zypp-core/zyppng/meta/Functional>
#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <zypp-core/zyppng/pipelines/Wait>
#include <zypp-core/zyppng/pipelines/Transform>
#include <zypp-core/zyppng/pipelines/operators.h>

namespace zyppng {

  template < typename T
           , typename Function
           , typename ResultType = std::invoke_result_t<Function, T>
           >
  ResultType and_then( const std::optional<T>& opt, Function &&f)
  {
      if (opt) {
        return std::invoke( std::forward<Function>(f), opt.value() );
      } else {
        if constexpr ( !detail::is_async_op< remove_smart_ptr_t<ResultType> >::value )
          return std::optional<ResultType>();
        else
          return makeReadyResult( std::optional<typename remove_smart_ptr_t<ResultType>::value_type>() );
      }
  }

  template < typename T
    , typename Function
    , typename ResultType = std::invoke_result_t<Function, T>
    >
  ResultType and_then( std::optional<T> &&opt, Function &&f)
  {
    if (opt) {
      return std::invoke( std::forward<Function>(f), std::move(opt.value()) );
    } else {
      if constexpr ( !detail::is_async_op< remove_smart_ptr_t<ResultType> >::value )
        return std::optional<ResultType>();
      else
        return makeReadyResult( std::optional<typename remove_smart_ptr_t<ResultType>::value_type>() );
    }
  }

  template < typename T
    , typename Function
    , typename ResultType = std::invoke_result_t<Function>
    >
  ResultType or_else( const std::optional<T>& opt, Function &&f)
  {
    if (!opt) {
      return std::invoke( std::forward<Function>(f), opt.error() );
    } else {
      if constexpr ( !detail::is_async_op< remove_smart_ptr_t<ResultType> >::value )
        return opt;
      else
        return makeReadyResult( std::move(opt) );
    }
  }

  template < typename T
    , typename Function
    , typename ResultType = std::invoke_result_t<Function>
    >
  ResultType or_else( std::optional<T>&& opt, Function &&f)
  {
    if (!opt) {
      return std::invoke( std::forward<Function>(f), std::move(opt.error()) );
    } else {
      if constexpr ( !detail::is_async_op< remove_smart_ptr_t<ResultType> >::value )
        return opt;
      else
        return makeReadyResult( std::move(opt) );
    }
  }
}
#endif
#endif
