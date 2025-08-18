/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaCurl2.h
 *
*/
#ifndef ZYPP_MEDIA_MEDIACURL2_H
#define ZYPP_MEDIA_MEDIACURL2_H

#include <zypp-core/ng/base/zyppglobal.h>
#include <zypp-core/base/Flags.h>
#include <zypp/ZYppCallbacks.h>
#include <zypp/media/MediaNetworkCommonHandler.h>

#include <curl/curl.h>

namespace zyppng {
  ZYPP_FWD_DECL_TYPE_WITH_REFS (EventDispatcher);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (NetworkRequestDispatcher);
  ZYPP_FWD_DECL_TYPE_WITH_REFS (NetworkRequest);
}

namespace zypp {

  namespace internal {
    ZYPP_FWD_DECL_TYPE_WITH_REFS (MediaNetworkRequestExecutor);
  }

  namespace media {

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : MediaCurl2
/**
 * @short Implementation class for FTP, HTTP and HTTPS MediaHandler
 * @see MediaHandler
 **/
class MediaCurl2 : public MediaNetworkCommonHandler
{
  public:
    enum RequestOption
    {
        /** Defaults */
        OPTION_NONE = 0x0,
      /** do not send a start ProgressReport */
        OPTION_NO_REPORT_START = 0x1
    };
    ZYPP_DECLARE_FLAGS(RequestOptions,RequestOption);

  protected:

    void releaseFrom( const std::string & ejectDev ) override;

    /**
     * Repeatedly calls doGetDoesFileExist() until it successfully returns,
     * fails unexpectedly, or user cancels the operation. This is used to
     * handle authentication or similar retry scenarios on media level.
     */
    bool getDoesFileExist( const Pathname & filename ) const override;

    /**
     *
     * \throws MediaException
     *
     */
    void disconnectFrom() override;
    /**
     *
     * \throws MediaException
     *
     */
    void getFileCopy( const OnMediaLocation& srcFile, const Pathname & targetFilename ) const override;
  public:

    MediaCurl2(const MirroredOrigin origin_r,
               const Pathname & attach_point_hint_r );

    ~MediaCurl2() override { try { release(); } catch(...) {} }

  protected:
    /**
     * check the url is supported by the curl library
     * \throws MediaBadUrlException if there is a problem
     **/
    void checkProtocol(const Url &url) const override;

  private:
    struct RequestData {
      int _mirrorIdx = -1;
      zyppng::NetworkRequestRef  _req;
    };
    void executeRequest(RequestData &reqData, callback::SendReport<DownloadProgressReport> *report = nullptr );

    bool tryZchunk( RequestData &reqData, const OnMediaLocation &srcFile , const Pathname & target, callback::SendReport<DownloadProgressReport> & report  );

  private:
    internal::MediaNetworkRequestExecutorRef _executor;
};
ZYPP_DECLARE_OPERATORS_FOR_FLAGS(MediaCurl2::RequestOptions);

///////////////////////////////////////////////////////////////////

  } // namespace media
} // namespace zypp

#endif // ZYPP_MEDIA_MEDIACURL2_H
