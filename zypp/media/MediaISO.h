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

#include "zypp/media/MediaHandler.h"
#include "zypp/media/MediaManager.h"

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
        MediaAccessId _isosource;
        std::string   _filesystem;

      protected:

	virtual void attachTo (bool next = false);
        virtual void releaseFrom( const std::string & ejectDev = "" );
	virtual void getFile( const Pathname & filename, const ByteCount &expectedFileSize_r ) const;
	virtual void getDir( const Pathname & dirname, bool recurse_r ) const;
        virtual void getDirInfo( std::list<std::string> & retlist,
                                 const Pathname & dirname, bool dots = true ) const;
        virtual void getDirInfo( filesystem::DirContent & retlist,
                                 const Pathname & dirname, bool dots = true ) const;
        virtual bool getDoesFileExist( const Pathname & filename ) const;

      public:

        MediaISO(const Url      &url_r,
                 const Pathname &attach_point_hint_r);

        virtual
        ~MediaISO();

        virtual bool
        isAttached() const;
    };


    //////////////////////////////////////////////////////////////////
  } // namespace media
  ////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////
} // namespace zypp
//////////////////////////////////////////////////////////////////////

#endif // ZYPP_MEDIA_MEDIAISO_H

// vim: set ts=2 sts=2 sw=2 ai et:

