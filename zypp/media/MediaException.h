/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaException.h
 *
*/
#ifndef ZYPP_MEDIA_MEDIAEXCEPTION_H
#define ZYPP_MEDIA_MEDIAEXCEPTION_H

#include <iosfwd>

#include <string>
#include <vector>

#include "zypp/base/Macros.h"
#include "zypp/base/Exception.h"
#include "zypp/Pathname.h"
#include "zypp/Url.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  namespace media {
    ///////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : MediaException
    /** Just inherits Exception to separate media exceptions
     *
     **/
    class ZYPP_API MediaException : public Exception
    {
    public:
      /** Ctor taking message.
       * Use \ref ZYPP_THROW to throw exceptions.
      */
      MediaException() : Exception( "Media Exception" )
      {}
      /** Ctor taking message.
       * Use \ref ZYPP_THROW to throw exceptions.
      */
      MediaException( const std::string & msg_r )
      : Exception( msg_r )
      {}

      /** Dtor. */
      virtual ~MediaException() throw() {};
    };

    class ZYPP_API MediaMountException : public MediaException
    {
    public:
      MediaMountException()
      : MediaException( "Media Mount Exception" )
      {}

      /** Ctor taking message.
       * Use \ref ZYPP_THROW to throw exceptions.
      */
      MediaMountException( const std::string & error_r,
			   const std::string & source_r,
			   const std::string & target_r,
			   const std::string & cmdout_r="")
      : MediaException()
      , _error(error_r)
      , _source(source_r)
      , _target(target_r)
      , _cmdout(cmdout_r)
      {}
      /** Dtor. */
      virtual ~MediaMountException() throw() {};

      const std::string & mountError() const
      { return _error;  }
      const std::string & mountSource() const
      { return _source; }
      const std::string & mountTarget() const
      { return _target; }
      const std::string & mountOutput() const
      { return _cmdout; }

    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _error;
      std::string _source;
      std::string _target;
      std::string _cmdout;
    };

    class ZYPP_API MediaUnmountException : public MediaException
    {
    public:
      /** Ctor taking message.
       * Use \ref ZYPP_THROW to throw exceptions.
      */
      MediaUnmountException( const std::string & error_r,
			     const std::string & path_r )
      : MediaException()
      , _error(error_r)
      , _path(path_r)
      {}
      /** Dtor. */
      virtual ~MediaUnmountException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _error;
      std::string _path;
    };

    class ZYPP_API MediaBadFilenameException : public MediaException
    {
    public:
      MediaBadFilenameException(const std::string & filename_r)
      : MediaException()
      , _filename(filename_r)
      {}
      virtual ~MediaBadFilenameException() throw() {};
      std::string filename() const { return _filename; }
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _filename;
    };

    class ZYPP_API MediaNotOpenException : public MediaException
    {
    public:
      MediaNotOpenException(const std::string & action_r)
      : MediaException()
      , _action(action_r)
      {}
      virtual ~MediaNotOpenException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _action;
    };

    class ZYPP_API MediaFileNotFoundException : public MediaException
    {
    public:
      MediaFileNotFoundException(const Url & url_r,
				 const Pathname & filename_r)
      : MediaException()
      , _url(url_r.asString())
      , _filename(filename_r.asString())
      {}
      virtual ~MediaFileNotFoundException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _url;
      std::string _filename;
    };

    class ZYPP_API MediaWriteException : public MediaException
    {
    public:
      MediaWriteException(const Pathname & filename_r)
      : MediaException()
      , _filename(filename_r.asString())
      {}
      virtual ~MediaWriteException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _filename;
    };

    class ZYPP_API MediaNotAttachedException : public MediaException
    {
    public:
      MediaNotAttachedException(const Url & url_r)
      : MediaException()
      , _url(url_r.asString())
      {}
      virtual ~MediaNotAttachedException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _url;
    };

    class ZYPP_API MediaBadAttachPointException : public MediaException
    {
    public:
      MediaBadAttachPointException(const Url & url_r)
      : MediaException()
      , _url(url_r.asString())
      {}
      virtual ~MediaBadAttachPointException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _url;
    };

    class ZYPP_API MediaCurlInitException : public MediaException
    {
    public:
      MediaCurlInitException(const Url & url_r)
      : MediaException()
      , _url(url_r.asString())
      {}
      virtual ~MediaCurlInitException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _url;
    };

class ZYPP_API MediaMetalinkInitException : public MediaException
    {
    public:
      MediaMetalinkInitException(const Url & url_r)
      : MediaException()
      , _url(url_r.asString())
      {}
      virtual ~MediaMetalinkInitException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _url;
    };

    class ZYPP_API MediaAria2cInitException : public MediaException
    {
    public:
      MediaAria2cInitException(const Url & url_r)
      : MediaException()
      , _url(url_r.asString())
      {}
      virtual ~MediaAria2cInitException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _url;
    };

    class ZYPP_API MediaSystemException : public MediaException
    {
    public:
      MediaSystemException(const Url & url_r,
			   const std::string & message_r)
      : MediaException()
      , _url(url_r.asString())
      , _message(message_r)
      {}
      virtual ~MediaSystemException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _url;
      std::string _message;
    };

    class ZYPP_API MediaNotAFileException : public MediaException
    {
    public:
      MediaNotAFileException(const Url & url_r,
			     const Pathname & path_r)
      : MediaException()
      , _url(url_r.asString())
      , _path(path_r.asString())
      {}
      virtual ~MediaNotAFileException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _url;
      std::string _path;
    };

    class ZYPP_API MediaNotADirException : public MediaException
    {
    public:
      MediaNotADirException(const Url & url_r,
			    const Pathname & path_r)
      : MediaException()
      , _url(url_r.asString())
      , _path(path_r.asString())
      {}
      virtual ~MediaNotADirException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _url;
      std::string _path;
    };

    class ZYPP_API MediaBadUrlException : public MediaException
    {
    public:
      MediaBadUrlException(const Url & url_r,
                           const std::string &msg_r = std::string())
      : MediaException()
      , _url(url_r.asString())
      , _msg(msg_r)
      {}
      virtual ~MediaBadUrlException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
      std::string _url;
      std::string _msg;
    };

    class ZYPP_API MediaBadUrlEmptyHostException : public MediaBadUrlException
    {
    public:
      MediaBadUrlEmptyHostException(const Url & url_r)
      : MediaBadUrlException(url_r)
      {}
      virtual ~MediaBadUrlEmptyHostException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    };

    class ZYPP_API MediaBadUrlEmptyFilesystemException : public MediaBadUrlException
    {
    public:
      MediaBadUrlEmptyFilesystemException(const Url & url_r)
      : MediaBadUrlException(url_r)
      {}
      virtual ~MediaBadUrlEmptyFilesystemException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    };

    class ZYPP_API MediaBadUrlEmptyDestinationException : public MediaBadUrlException
    {
    public:
      MediaBadUrlEmptyDestinationException(const Url & url_r)
      : MediaBadUrlException(url_r)
      {}
      virtual ~MediaBadUrlEmptyDestinationException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    };

    class ZYPP_API MediaUnsupportedUrlSchemeException : public MediaBadUrlException
    {
    public:
      MediaUnsupportedUrlSchemeException(const Url & url_r)
      : MediaBadUrlException(url_r)
      {}
      virtual ~MediaUnsupportedUrlSchemeException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    };

    class ZYPP_API MediaNotSupportedException : public MediaException
    {
    public:
      MediaNotSupportedException(const Url & url_r)
      : MediaException()
      , _url(url_r.asString())
      {}
      virtual ~MediaNotSupportedException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
      std::string _url;
    };

    class ZYPP_API MediaCurlException : public MediaException
    {
    public:
      MediaCurlException(const Url & url_r,
			 const std::string & err_r,
			 const std::string & msg_r)
      : MediaException()
      , _url(url_r.asString())
      , _err(err_r)
      , _msg(msg_r)
      {}
      virtual ~MediaCurlException() throw() {};
      std::string errstr() const { return _err; }
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
      std::string _url;
      std::string _err;
      std::string _msg;
    };

    class ZYPP_API MediaCurlSetOptException : public MediaException
    {
    public:
      MediaCurlSetOptException(const Url & url_r, const std::string & msg_r)
      : MediaException()
      , _url(url_r.asString())
      , _msg(msg_r)
      {}
      virtual ~MediaCurlSetOptException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
      std::string _url;
      std::string _msg;
    };

    class ZYPP_API MediaMetalinkException : public MediaException
    {
    public:
      MediaMetalinkException(const Url & url_r,
			 const std::string & err_r,
			 const std::string & msg_r)
      : MediaException()
      , _url(url_r.asString())
      , _err(err_r)
      , _msg(msg_r)
      {}
      virtual ~MediaMetalinkException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
      std::string _url;
      std::string _err;
      std::string _msg;
    };

    class ZYPP_API MediaMetalinkSetOptException : public MediaException
    {
    public:
      MediaMetalinkSetOptException(const Url & url_r, const std::string & msg_r)
      : MediaException()
      , _url(url_r.asString())
      , _msg(msg_r)
      {}
      virtual ~MediaMetalinkSetOptException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
      std::string _url;
      std::string _msg;
    };

    class ZYPP_API MediaAria2cException : public MediaException
    {
    public:
      MediaAria2cException(const Url & url_r,
			 const std::string & err_r,
			 const std::string & msg_r)
      : MediaException()
      , _url(url_r.asString())
      , _err(err_r)
      , _msg(msg_r)
      {}
      virtual ~MediaAria2cException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
      std::string _url;
      std::string _err;
      std::string _msg;
    };

    class ZYPP_API MediaAria2cSetOptException : public MediaException
    {
    public:
      MediaAria2cSetOptException(const Url & url_r, const std::string & msg_r)
      : MediaException()
      , _url(url_r.asString())
      , _msg(msg_r)
      {}
      virtual ~MediaAria2cSetOptException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
      std::string _url;
      std::string _msg;
    };

    class ZYPP_API MediaNotDesiredException : public MediaException
    {
    public:
      MediaNotDesiredException(const Url & url_r)
      : MediaException()
      , _url(url_r.asString())
      {}
      virtual ~MediaNotDesiredException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string  _url;
    };

    class ZYPP_API MediaIsSharedException : public MediaException
    {
    public:
      /**
       * \param name A media source as string (see MediaSource class).
       */
      MediaIsSharedException(const std::string &name)
      : MediaException()
      , _name(name)
      {}
      virtual ~MediaIsSharedException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _name;
    };

    class ZYPP_API MediaNotEjectedException: public MediaException
    {
    public:
      MediaNotEjectedException()
      : MediaException("Can't eject any media")
      , _name("")
      {}

      MediaNotEjectedException(const std::string &name)
      : MediaException("Can't eject media")
      , _name(name)
      {}
      virtual ~MediaNotEjectedException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      std::string _name;
    };

    class ZYPP_API MediaUnauthorizedException: public MediaException
    {
    public:
      MediaUnauthorizedException()
      : MediaException("Unauthorized media access")
      , _url("")
      , _err("")
      , _hint("")
      {}

      MediaUnauthorizedException(const Url         &url_r,
                                 const std::string &msg_r,
                                 const std::string &err_r,
                                 const std::string &hint_r)
      : MediaException(msg_r)
      , _url(url_r)
      , _err(err_r)
      , _hint(hint_r)
      {}

      virtual ~MediaUnauthorizedException() throw() {};

      const Url         & url()  const { return _url;  }
      const std::string & err()  const { return _err;  }
      /** comma separated list of available authentication types */
      const std::string & hint() const { return _hint; }

    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
    private:
      Url         _url;
      std::string _err;
      std::string _hint;
    };

    class ZYPP_API MediaForbiddenException : public MediaException
    {
    public:
      MediaForbiddenException(const Url & url_r, const std::string & msg = "")
      : MediaException(msg)
      , _url(url_r.asString()), _msg(msg)
      {}
      virtual ~MediaForbiddenException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
      std::string _url;
      std::string _msg;
    };

    class ZYPP_API MediaTimeoutException : public MediaException
    {
    public:
      MediaTimeoutException(const Url & url_r, const std::string & msg = "")
      : MediaException(msg)
      , _url(url_r.asString()), _msg(msg)
      {}
      virtual ~MediaTimeoutException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
      std::string _url;
      std::string _msg;
    };

    /** For HTTP 503 and similar. */
    class ZYPP_API MediaTemporaryProblemException : public MediaException
    {
    public:
      MediaTemporaryProblemException(const Url & url_r, const std::string & msg = "")
      : MediaException(msg)
      , _url(url_r.asString()), _msg(msg)
      {}
      virtual ~MediaTemporaryProblemException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
      std::string _url;
      std::string _msg;
    };

    class ZYPP_API MediaBadCAException : public MediaException
    {
    public:
      MediaBadCAException(const Url & url_r, const std::string & msg = "")
      : MediaException(msg)
      , _url(url_r.asString()), _msg(msg)
      {}
      virtual ~MediaBadCAException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
      std::string _url;
      std::string _msg;
    };

    /**
     * Thrown if /sbin/losetup fails to find an unused loop device for mounting
     * an .iso image.
     *
     * UI hint: tell user to check permissions to read /dev/loop# or enablement
     * of support for loop devices.
     *
     * \see MediaISO
     */
    class ZYPP_API MediaNoLoopDeviceException : public MediaException
    {
    public:
      MediaNoLoopDeviceException(const Url & url_r, const std::string & msg = "")
        : MediaException(msg)
        , _url(url_r.asString()), _msg(msg)
      {}
      virtual ~MediaNoLoopDeviceException() throw() {};
    protected:
      virtual std::ostream & dumpOn( std::ostream & str ) const;
      std::string _url;
      std::string _msg;
    };
  /////////////////////////////////////////////////////////////////
  } // namespace media
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_MEDIA_MEDIAEXCEPTION_H
