/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaISO.h
 *
 */
#ifndef ZYPP_MEDIA_MEDIAISO_H
#define ZYPP_MEDIA_MEDIAISO_H

#include <zypp/media/MediaHandler.h>
#include <zypp/media/MediaManager.h>

//////////////////////////////////////////////////////////////////////
namespace zypp
{ ////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////
  namespace media
  { //////////////////////////////////////////////////////////////////


    ///////////////////////////////////////////////////////////////////
    //
    // CLASS NAME : MediaISO
    //
    /**
     * @short Implementation class for ISO MediaHandler
     * @see MediaHandler
     **/
    class MediaISO : public MediaHandler
    {
      private:
        Pathname      _isofile;
        std::string   _filesystem;

      protected:

        void attachTo (bool next = false) override;
        void releaseFrom( const std::string & ejectDev = "" ) override;
        void getFile( const OnMediaLocation & file ) const override;
        void getDir( const Pathname & dirname, bool recurse_r ) const override;
        void getDirInfo( std::list<std::string> & retlist,
                                 const Pathname & dirname, bool dots = true ) const override;
        void getDirInfo( filesystem::DirContent & retlist,
                                 const Pathname & dirname, bool dots = true ) const override;
        bool getDoesFileExist( const Pathname & filename ) const override;

      public:

        MediaISO(const MirroredOrigin &origin_r,
                 const Pathname &attach_point_hint_r);


        ~MediaISO() override;

        bool
        isAttached() const override;
    };


    //////////////////////////////////////////////////////////////////
  } // namespace media
  ////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////
} // namespace zypp
//////////////////////////////////////////////////////////////////////

#endif // ZYPP_MEDIA_MEDIAISO_H

// vim: set ts=2 sts=2 sw=2 ai et:

