/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/base/Function.h
 *
*/
#ifndef ZYPP_BASE_FUNCTION_H
#define ZYPP_BASE_FUNCTION_H

#include <functional>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  /* This header is no longer needed, it was meant to import
   * boost::function. This is replaced by C++11's std::function
   *
   * TODO: 1) Remove this header. 2) Include <functional> instead.
   *       3) Fully qualify std::function and std:: bind everywhere.
   */
  using std::function;

  using std::bind;
  using std::placeholders::_1;
  using std::placeholders::_2;
  using std::placeholders::_3;

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_BASE_FUNCTION_H
