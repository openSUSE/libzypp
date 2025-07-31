/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/PluginFrameException.cc
 *
*/
#include <iostream>
//#include <zypp-core/base/LogTools.h>

#include <zypp-core/rpc/PluginFrameException.h>
#include <zypp-core/rpc/PluginFrame.h>

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  PluginFrameException::PluginFrameException()
    : Exception( "PluginFrameException" )
  {}

  PluginFrameException::PluginFrameException( const std::string & msg_r )
    : Exception( msg_r )
  {}

  PluginFrameException::PluginFrameException( const std::string & msg_r, const std::string & hist_r )
    : Exception( msg_r )
  { addHistory( hist_r ); }

  PluginFrameException::~PluginFrameException() throw()
  {};

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
