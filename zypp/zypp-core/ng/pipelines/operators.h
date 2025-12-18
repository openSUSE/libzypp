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

#ifndef ZYPP_ZYPPNG_PIPELINE_OPERATORS_H
#define ZYPP_ZYPPNG_PIPELINE_OPERATORS_H


#include <utility>
namespace zyppng::operators {

  template< typename In , typename Callback>
  std::invoke_result_t<Callback,In> operator| ( In &&in, Callback &&c )
  {
    return std::forward<Callback>(c)( std::forward<In>(in) );
  }

}

#endif
