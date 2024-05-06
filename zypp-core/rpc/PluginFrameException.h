/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/PluginFrameException.h
 *
*/
#ifndef ZYPP_CORE_PLUGINFRAMEEXCEPTION_H
#define ZYPP_CORE_PLUGINFRAMEEXCEPTION_H

#include <zypp/base/Exception.h>
#include <zypp/Pathname.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : PluginFrameException
  //
  /** Base class for \ref PluginFrame \ref Exception. */
  class ZYPP_API PluginFrameException : public Exception
  {
    public:
      PluginFrameException();
      PluginFrameException( const std::string & msg_r );
      PluginFrameException( const std::string & msg_r, const std::string & hist_r );
      ~PluginFrameException() throw() override;
  };
  ///////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_CORE_PLUGINFRAMEEXCEPTION_H
