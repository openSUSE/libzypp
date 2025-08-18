/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/media/UrlResolverPlugin.h
 *
*/
#ifndef ZYPP_MEDIA_URLRESOLVERPLUGIN_H
#define ZYPP_MEDIA_URLRESOLVERPLUGIN_H

#include <iosfwd>
#include <map>
#include <string>

#include <zypp-core/Globals.h>
#include <zypp-core/base/PtrTypes.h>
#include <zypp-core/Url.h>
#include <zypp/PathInfo.h>
///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace media
  { /////////////////////////////////////////////////////////////////

    // openSUSE/libzypp#560: Keep it public while foreign test depend on it.
    class ZYPP_API UrlResolverPlugin
    {
      friend std::ostream & operator<<( std::ostream & str, const UrlResolverPlugin & obj );

    public:

      struct Impl;

      using HeaderList = std::multimap<std::string, std::string>;

      /**
       * Resolves an url using the installed plugins
       * If no plugin is found the url is resolved as
       * its current value.
       *
       * Custom headers are inserted in the provided header list
       */
      static Url resolveUrl(const Url &url, HeaderList &headers);

    public:
      /** Dtor */
      ~UrlResolverPlugin();

    private:

      /** Default ctor */
      UrlResolverPlugin();

      /** Pointer to implementation */
      RW_pointer<Impl> _pimpl;
    };
    ///////////////////////////////////////////////////////////////////

    /** \relates UrlResolverPlugin Stream output */
    std::ostream & operator<<( std::ostream & str, const UrlResolverPlugin & obj );

    /////////////////////////////////////////////////////////////////
  } // namespace media
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_MEDIA_URLRESOLVERPLUGIN_H
