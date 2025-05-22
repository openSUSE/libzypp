/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/media/MediaPriority.cc
 *
*/
#include <iostream>
#include <zypp/base/LogTools.h>

#include <zypp/Url.h>
#include <zypp/ZConfig.h>

#include <zypp/media/MediaPriority.h>

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace media
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    namespace
    { /////////////////////////////////////////////////////////////////

      /**
       * 4: local:     file,dir,hd
       * 3: remote:    nfs,nfs4,cifs,smb
       * ?: download:  http,https,ftp,sftp, tftp
       * ?: volatile:  cd,dvd
       * 0:            the rest
      */
      MediaPriority::value_type scheme2priority(  const std::string & scheme_r )
      {
        switch ( scheme_r[0] )
        {
          case 'c':
            if ( scheme_r.compare("cd") == 0 )
            {
              return ZConfig::instance().download_media_prefer_download() ? 1 : 2;
            }
            if ( scheme_r.compare("cifs") == 0 )
            {
              return 3;
            }
            break;

          case 'd':
            if ( scheme_r.compare("dvd") == 0 )
            {
              return ZConfig::instance().download_media_prefer_download() ? 1 : 2;
            }
            if ( scheme_r.compare("dir") == 0 )
            {
              return 4;
            }
            break;

          case 'f':
            if ( scheme_r.compare("ftp") == 0 )
            {
              return ZConfig::instance().download_media_prefer_download() ? 2 : 1;
            }
            if ( scheme_r.compare("file") == 0 )
            {
              return 4;
            }
            break;

          case 't':
            if ( scheme_r.compare("tftp") == 0 )
            {
              return ZConfig::instance().download_media_prefer_download() ? 2 : 1;
            }
            break;

          case 'h':
            if ( (scheme_r.compare("http") == 0 )
                 || ( scheme_r.compare("https") == 0 ) )
            {
              return ZConfig::instance().download_media_prefer_download() ? 2 : 1;
            }
            if ( scheme_r.compare("hd") == 0 )
            {
              return 4;
            }
            break;

          case 'n':
            if ( (scheme_r.compare("nfs") == 0 )
                 || ( scheme_r.compare("nfs4") == 0 ) )
            {
              return 3;
            }
            break;

          case 's':
            if (scheme_r.compare("sftp") == 0 )
            {
              return ZConfig::instance().download_media_prefer_download() ? 2 : 1;
            }
            if ( scheme_r.compare("smb") == 3 )
            {
              return 4;
            }
            break;
        }
        // the rest
        return 0;
      }

      /////////////////////////////////////////////////////////////////
    } // namespace
    ///////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : MediaPriority::MediaPriority
    //	METHOD TYPE : Ctor
    //
    MediaPriority::MediaPriority( const std::string & scheme_r )
      : _val( scheme2priority( scheme_r ) )
    {}

    MediaPriority::MediaPriority( const Url & url_r )
      : _val( scheme2priority( url_r.getScheme() ) )
    {}

    /////////////////////////////////////////////////////////////////
  } // namespace media
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
