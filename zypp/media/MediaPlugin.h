/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaPlugin.h
 *
*/
#ifndef ZYPP_MEDIA_MEDIAPLUGIN_H
#define ZYPP_MEDIA_MEDIAPLUGIN_H

#include <zypp/media/MediaHandler.h>

//////////////////////////////////////////////////////////////////////
namespace zypp
{ ////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////
  namespace media
  { //////////////////////////////////////////////////////////////////

    /**
     * \brief Implementation class for plugin MediaHandler
     *
     * @see MediaHandler
     */
    class MediaPlugin : public MediaHandler
    {
      public:
        MediaPlugin( const Url & url_r, const Pathname & attach_point_hint_r );

        ~MediaPlugin() override { try { release(); } catch(...) {} }

      protected:
        void attachTo( bool next_r = false ) override;
        void releaseFrom( const std::string & ejectDev_r ) override;
        void getFile( const OnMediaLocation & file ) const override;
        void getDir( const Pathname & dirname_r, bool recurse_r ) const override;
        void getDirInfo( std::list<std::string> & retlist_r, const Pathname & dirname_r, bool dots_r = true ) const override;
        void getDirInfo( filesystem::DirContent & retlist_r, const Pathname & dirname_r, bool dots_r = true ) const override;
        bool getDoesFileExist( const Pathname & filename_r ) const override;
    };

    //////////////////////////////////////////////////////////////////
  } // namespace media
  ////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////
} // namespace zypp
//////////////////////////////////////////////////////////////////////
#endif // ZYPP_MEDIA_MEDIAPLUGIN_H
