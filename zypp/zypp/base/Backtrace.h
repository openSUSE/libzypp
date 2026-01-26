/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/base/Backtrace.h
 */
#ifndef ZYPP_BASE_BACKTRACE_H
#define ZYPP_BASE_BACKTRACE_H

#include <iosfwd>
#include <string>

#include <zypp-core/Globals.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{

  /** Dump current stack trace to a stream.
   * Thanks to http://stackoverflow.com/questions/77005.
    * \code
   * #include <iostream>
   * std::cerr << zypp::dumpBacktrace << std::endl;
   * \endcode
   * \code
   * #include <zypp-core/base/String.h>
   * std::string trace( str::Str() << zypp::dumpBacktrace );
   * \endcode
   */
  std::ostream & dumpBacktrace( std::ostream & stream_r ) ZYPP_API;

} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_BASE_BACKTRACE_H
