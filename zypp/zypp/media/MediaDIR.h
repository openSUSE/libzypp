/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaDIR.h
 *
*/
#ifndef ZYPP_MEDIA_MEDIADIR_H
#define ZYPP_MEDIA_MEDIADIR_H

#include <zypp/media/MediaHandler.h>

namespace zypp {
  namespace media {

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : MediaDIR

    /**
     * @short Implementation class for DIR MediaHandler
     * @see MediaHandler
     **/
    class MediaDIR : public MediaHandler {

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

        MediaDIR(const MirroredOrigin &origin,
                 const Pathname & attach_point_hint_r );

        ~MediaDIR() override { try { release(); } catch(...) {} }
    };

    ///////////////////////////////////////////////////////////////////

  } // namespace media
} // namespace zypp

#endif // ZYPP_MEDIA_MEDIADIR_H
