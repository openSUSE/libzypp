/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-media/MediaException
 *
*/
#ifndef ZYPP_MEDIA_MEDIAEXCEPTION_H
#define ZYPP_MEDIA_MEDIAEXCEPTION_H

#include <iosfwd>

#include <string>
#include <utility>
#include <vector>

#include <zypp-core/base/Exception.h>
#include <zypp-core/Pathname.h>
#include <zypp-core/Url.h>
#include <zypp-core/ByteCount.h>

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
      ~MediaException() noexcept override;
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
      MediaMountException( std::string  error_r,
                           std::string  source_r,
                           std::string  target_r,
                           std::string  cmdout_r="")
      : MediaException()
      , _error(std::move(error_r))
      , _source(std::move(source_r))
      , _target(std::move(target_r))
      , _cmdout(std::move(cmdout_r))
      {}
      /** Dtor. */
      ~MediaMountException() noexcept override {}

      const std::string & mountError() const
      { return _error;  }
      const std::string & mountSource() const
      { return _source; }
      const std::string & mountTarget() const
      { return _target; }
      const std::string & mountOutput() const
      { return _cmdout; }

    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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
      MediaUnmountException( std::string  error_r,
                             std::string  path_r )
      : MediaException()
      , _error(std::move(error_r))
      , _path(std::move(path_r))
      {}
      /** Dtor. */
      ~MediaUnmountException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
    private:
      std::string _error;
      std::string _path;
    };

    class ZYPP_API MediaJammedException : public MediaException
    {
    public:
      /** Ctor taking message.
       * Use \ref ZYPP_THROW to throw exceptions.
      */
      MediaJammedException() : MediaException( "Media Jammed Exception" )
      {}

      /** Dtor. */
      ~MediaJammedException() noexcept override {};
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;

    private:
    };

    class ZYPP_API MediaBadFilenameException : public MediaException
    {
    public:
      MediaBadFilenameException(std::string  filename_r)
      : MediaException()
      , _filename(std::move(filename_r))
      {}
      ~MediaBadFilenameException() noexcept override {}
      std::string filename() const { return _filename; }
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
    private:
      std::string _filename;
    };

    class ZYPP_API MediaNotOpenException : public MediaException
    {
    public:
      MediaNotOpenException(std::string  action_r)
      : MediaException()
      , _action(std::move(action_r))
      {}
      ~MediaNotOpenException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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
      ~MediaFileNotFoundException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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
      ~MediaWriteException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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
      ~MediaNotAttachedException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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
      ~MediaBadAttachPointException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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
      ~MediaCurlInitException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
    private:
      std::string _url;
    };

    class ZYPP_API MediaSystemException : public MediaException
    {
    public:
      MediaSystemException(const Url & url_r,
                           std::string  message_r)
      : MediaException()
      , _url(url_r.asString())
      , _message(std::move(message_r))
      {}
      ~MediaSystemException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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
      ~MediaNotAFileException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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
      ~MediaNotADirException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
    private:
      std::string _url;
      std::string _path;
    };

    class ZYPP_API MediaBadUrlException : public MediaException
    {
    public:
      MediaBadUrlException(const Url & url_r,
                           std::string msg_r = std::string())
      : MediaException()
      , _url(url_r.asString())
      , _msg(std::move(msg_r))
      {}
      ~MediaBadUrlException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
      std::string _url;
      std::string _msg;
    };

    class ZYPP_API MediaBadUrlEmptyHostException : public MediaBadUrlException
    {
    public:
      MediaBadUrlEmptyHostException(const Url & url_r)
      : MediaBadUrlException(url_r)
      {}
      ~MediaBadUrlEmptyHostException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
    };

    class ZYPP_API MediaBadUrlEmptyFilesystemException : public MediaBadUrlException
    {
    public:
      MediaBadUrlEmptyFilesystemException(const Url & url_r)
      : MediaBadUrlException(url_r)
      {}
      ~MediaBadUrlEmptyFilesystemException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
    };

    class ZYPP_API MediaBadUrlEmptyDestinationException : public MediaBadUrlException
    {
    public:
      MediaBadUrlEmptyDestinationException(const Url & url_r)
      : MediaBadUrlException(url_r)
      {}
      ~MediaBadUrlEmptyDestinationException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
    };

    class ZYPP_API MediaUnsupportedUrlSchemeException : public MediaBadUrlException
    {
    public:
      MediaUnsupportedUrlSchemeException(const Url & url_r)
      : MediaBadUrlException(url_r)
      {}
      ~MediaUnsupportedUrlSchemeException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
    };

    class ZYPP_API MediaNotSupportedException : public MediaException
    {
    public:
      MediaNotSupportedException(const Url & url_r)
      : MediaException()
      , _url(url_r.asString())
      {}
      ~MediaNotSupportedException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
      std::string _url;
    };

    class ZYPP_API MediaCurlException : public MediaException
    {
    public:
      MediaCurlException(const Url & url_r,
                         std::string  err_r,
                         std::string  msg_r)
      : MediaException()
      , _url(url_r.asString())
      , _err(std::move(err_r))
      , _msg(std::move(msg_r))
      {}
      ~MediaCurlException() noexcept override {}
      std::string errstr() const { return _err; }
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
      std::string _url;
      std::string _err;
      std::string _msg;
    };

    class ZYPP_API MediaCurlSetOptException : public MediaException
    {
    public:
      MediaCurlSetOptException(const Url & url_r, std::string  msg_r)
      : MediaException()
      , _url(url_r.asString())
      , _msg(std::move(msg_r))
      {}
      ~MediaCurlSetOptException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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
      ~MediaNotDesiredException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
    private:
      std::string  _url;
    };

    class ZYPP_API MediaIsSharedException : public MediaException
    {
    public:
      /**
       * \param name A media source as string (see MediaSource class).
       */
      MediaIsSharedException(std::string name)
      : MediaException()
      , _name(std::move(name))
      {}
      ~MediaIsSharedException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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

      MediaNotEjectedException(std::string name)
      : MediaException("Can't eject media")
      , _name(std::move(name))
      {}
      ~MediaNotEjectedException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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

      MediaUnauthorizedException(Url url_r,
                                 const std::string &msg_r,
                                 std::string err_r,
                                 std::string hint_r)
      : MediaException(msg_r)
      , _url(std::move(url_r))
      , _err(std::move(err_r))
      , _hint(std::move(hint_r))
      {}

      ~MediaUnauthorizedException() noexcept override {}

      const Url         & url()  const { return _url;  }
      const std::string & err()  const { return _err;  }
      /** comma separated list of available authentication types */
      const std::string & hint() const { return _hint; }

    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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
      ~MediaForbiddenException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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
      ~MediaTimeoutException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
      std::string _url;
      std::string _msg;
    };

    class ZYPP_API MediaFileSizeExceededException : public MediaException
    {
    public:
      MediaFileSizeExceededException(const Url & url_r, const ByteCount &cnt_r, const std::string & msg = "")
      : MediaException(msg)
      , _url(url_r.asString())
      , _msg(msg)
      , _expectedFileSize(cnt_r)
      {}
      ~MediaFileSizeExceededException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
      std::string _url;
      std::string _msg;
      ByteCount _expectedFileSize;
    };

    /** For HTTP 503 and similar. */
    class ZYPP_API MediaTemporaryProblemException : public MediaException
    {
    public:
      MediaTemporaryProblemException(const Url & url_r, const std::string & msg = "")
      : MediaException(msg)
      , _url(url_r.asString()), _msg(msg)
      {}
      ~MediaTemporaryProblemException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
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
      ~MediaBadCAException() noexcept override {}
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
      std::string _url;
      std::string _msg;
    };

    class ZYPP_API MediaInvalidCredentialsException : public MediaException
    {
    public:
      MediaInvalidCredentialsException( const std::string & msg = "" )
        : MediaException(msg)
      {}
      ~MediaInvalidCredentialsException() noexcept override {}
    };

    class ZYPP_API MediaRequestCancelledException : public MediaException
    {
    public:
      MediaRequestCancelledException( const std::string & msg = "" )
        : MediaException(msg)
      {}
      ~MediaRequestCancelledException() noexcept override {}
    };

  /////////////////////////////////////////////////////////////////
  } // namespace media
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_MEDIA_MEDIAEXCEPTION_H
