/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaDISK.h
 *
*/
#ifndef ZYPP_MEDIA_MEDIADISK_H
#define ZYPP_MEDIA_MEDIADISK_H

#include <zypp/media/MediaHandler.h>

namespace zypp {
  namespace media {

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : MediaDISK
    /**
     * @short Implementation class for DISK MediaHandler
     * @see MediaHandler
     **/
    class MediaDISK : public MediaHandler {

      private:
        std::string _device;
        std::string _filesystem;

      protected:

        void attachTo (bool next = false) override;
        void releaseFrom( const std::string & ejectDev ) override;
        void getFile( const OnMediaLocation & file ) const override;
        void getDir( const Pathname & dirname, bool recurse_r ) const override;
        void getDirInfo( std::list<std::string> & retlist,
                                 const Pathname & dirname, bool dots = true ) const override;
        void getDirInfo( filesystem::DirContent & retlist,
                                 const Pathname & dirname, bool dots = true ) const override;
        bool getDoesFileExist( const Pathname & filename ) const override;

      public:

        MediaDISK( const MediaUrl &      url_r,
                   const Pathname & attach_point_hint_r );

        ~MediaDISK() override { try { release(); } catch(...) {} }

        bool isAttached() const override;

        bool    verifyIfDiskVolume(const Pathname &name);
    };

///////////////////////////////////////////////////////////////////

  } // namespace media
} // namespace zypp

#endif // ZYPP_MEDIA_MEDIADISK_H
