/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaCurl.h
 *
*/
#ifndef ZYPP_MEDIA_MEDIACURL_H
#define ZYPP_MEDIA_MEDIACURL_H

#include <zypp/base/Flags.h>
#include <zypp/ZYppCallbacks.h>
#include <zypp/media/MediaNetworkCommonHandler.h>
#include <zypp-curl/private/curlhelper_p.h>

#include <curl/curl.h>

namespace zypp {
  namespace media {

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : MediaCurl
/**
 * @short Implementation class for FTP, HTTP and HTTPS MediaHandler
 * @see MediaHandler
 **/
class MediaCurl : public MediaNetworkCommonHandler, public internal::CurlPollHelper::CurlPoll
{
  struct RequestData {
    int mirror = -1;
    CURL *curl = nullptr;
  };

  protected:

    void releaseFrom( const std::string & ejectDev ) override;

    /**
     * Repeatedly calls doGetDoesFileExist() until it successfully returns,
     * fails unexpectedly, or user cancels the operation. This is used to
     * handle authentication or similar retry scenarios on media level.
     */
    bool getDoesFileExist( const Pathname & filename ) const override;

    /**
     * \see MediaHandler::getDoesFileExist
     */
    bool doGetDoesFileExist( const int mirror, const Pathname & filename );

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
    void getFileCopy( const OnMediaLocation& srcFile, const Pathname & target ) const override;

    void getFileCopyFromMirror(const int mirror, const OnMediaLocation& srcFile, const Pathname & target );

  public:

    MediaCurl( const MirroredOrigin &origin_r,
               const Pathname &attach_point_hint_r );

    ~MediaCurl() override;

    static void setCookieFile( const Pathname & );

  protected:
    /** Callback sending just an alive trigger to the UI, without stats (e.g. during metalink download). */
    static int aliveCallback( void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow );
    /** Callback reporting download progress. */
    static int progressCallback( void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow );
    /** Callback writing the data into our file */
    static size_t writeCallback( char *ptr, size_t size, size_t nmemb, void *userdata );

    void checkProtocol(const Url &url) const override;

    /**
     * initializes the curl easy handle with the data from the url
     * \throws MediaCurlSetOptException if there is a problem
     **/
    void setupEasy(RequestData &rData, TransferSettings &settings );

    /**
     * Evaluates a curl return code and throws the right MediaException
     * \p filename Filename being downloaded
     * \p code Code curl returnes
     * \p timeout Whether we reached timeout, which we need to differentiate
     *    in case the codes aborted-by-callback or timeout are returned by curl
     *    Otherwise we can't differentiate abort from timeout. Here you may
     *    want to pass the progress data object timeout-reached value, or
     *    just true if you are not doing user interaction.
     *
     * \throws MediaException If there is a problem
     */
    void evaluateCurlCode( RequestData &rData, const zypp::Pathname &fileName, CURLcode code, bool timeout ) const;

    static void resetExpectedFileSize ( void *clientp, const ByteCount &expectedFileSize );

  private:

    CURLcode executeCurl( RequestData &rData );

    /**
     * Return a comma separated list of available authentication methods
     * supported by server.
     */
    std::string getAuthHint(CURL *curl) const;

    //bool authenticate(const std::string & availAuthTypes, bool firstTry) const;

    bool detectDirIndex() const;

  private:
    std::string _currentCookieFile;
    static Pathname _cookieFile;
    char _curlError[ CURL_ERROR_SIZE ];

    mutable std::string _lastRedirect;	///< to log/report redirections
    curl_slist *_customHeaders;

    const char* curlError() const { return _curlError; };
    void setCurlError(const char* error);
};

///////////////////////////////////////////////////////////////////

  } // namespace media
} // namespace zypp

#endif // ZYPP_MEDIA_MEDIACURL_H
