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

#ifndef ZYPP_ZYPPNG_MONADIC_WAIT_H
#define ZYPP_ZYPPNG_MONADIC_WAIT_H

#include <zypp-core/zyppng/pipelines/AsyncResult>
#include <functional>

namespace zyppng {

namespace detail {

  template < template< class, class... > class Container,
    class AsyncOp,
    class ...CArgs
    >
  struct WaitForImpl : public zyppng::AsyncOp< Container<typename AsyncOp::value_type> > {

    using AsyncOpRes = typename AsyncOp::value_type;

    static_assert( detail::is_async_op_v<AsyncOp>, "Result type needs to be derived from AsyncOp");

    WaitForImpl ( std::function<bool( const AsyncOpRes &)> canContinue = {} )
      : _canContinue( std::move(canContinue) ){};

    WaitForImpl ( const WaitForImpl &other ) = delete;
    WaitForImpl& operator= ( const WaitForImpl &other ) = delete;

    WaitForImpl& operator= ( WaitForImpl &&other ) = default;
    WaitForImpl ( WaitForImpl &&other ) = default;

    void operator()( Container< std::shared_ptr<AsyncOp>, CArgs...> &&ops ) {
      assert( _allOps.empty() );

      if ( ops.empty () ) {
        this->setReady( std::move(_allResults) );
      }

      _allOps = std::move( ops );
      for ( auto &op : _allOps ) {
        op->onReady( [ this ](  AsyncOpRes &&res  )  {
          this->resultReady( std::move(res));
        });
      }
    }

  private:
    void resultReady ( AsyncOpRes &&res ) {
      _allResults.push_back( std::move( res ) );

      bool done = ( _allOps.size() == _allResults.size()) || ( _canContinue && !_canContinue(_allResults.back()));
      if ( done ) {
        //release all ops we waited on
        _allOps.clear();

        this->setReady( std::move(_allResults) );
      }
    }

    Container< std::shared_ptr<AsyncOp>, CArgs... > _allOps;
    Container< AsyncOpRes > _allResults;
    std::function<bool( const AsyncOpRes &)> _canContinue;
  };

  struct WaitForHelper
  {
    template<
      template< class, class... > class Container,
      class AsyncOp,
      typename ...CArgs,
      std::enable_if_t< detail::is_async_op_v<AsyncOp>, int> = 0
      >
    auto operator()( Container< std::shared_ptr< AsyncOp >, CArgs... > &&ops ) -> zyppng::AsyncOpRef< Container<typename AsyncOp::value_type> > {
      auto aOp = std::make_shared<detail::WaitForImpl<Container, AsyncOp, CArgs...>>( );
      aOp->operator()( std::move(ops) );
      return aOp;
    }

    template<
      template< class, class... > class Container,
      class Res,
      typename ...CArgs,
      std::enable_if_t< !detail::is_async_op_v<Res>, int> = 0
      >
    auto operator()( Container< Res, CArgs... > &&ops ) -> Container< Res, CArgs... > {
      return ops;
    }
  };


  template <typename AsyncOpRes>
  struct WaitForHelperExt
  {
    WaitForHelperExt( std::function<bool( const AsyncOpRes &)> &&fn ) : _cb( std::move(fn) ) {}

    template<
      template< class, class... > class Container,
      class AsyncOp,
      typename ...CArgs,
      std::enable_if_t< detail::is_async_op_v<AsyncOp>, int> = 0
      >
    auto operator()( Container< std::shared_ptr< AsyncOp >, CArgs... > &&ops ) -> zyppng::AsyncOpRef< Container<typename AsyncOp::value_type> > {
      auto aOp = std::make_shared<detail::WaitForImpl<Container, AsyncOp, CArgs...>>( _cb );
      aOp->operator()( std::move(ops) );
      return aOp;
    }

    private:
      std::function<bool( const AsyncOpRes &)> _cb;
  };

}

/*!
 *  Returns a async operation that waits for all async operations which are passed to it and collects their results,
 *  forwarding them as one
 */
inline auto waitFor ( ) {
  return detail::WaitForHelper(); //std::make_shared<detail::WaitForImpl<zyppng::AsyncOp<Res>>>();
}

inline auto join ( ) {
  return detail::WaitForHelper();
}

}
#endif
