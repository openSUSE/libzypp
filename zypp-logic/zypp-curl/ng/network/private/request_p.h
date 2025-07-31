/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPP_NG_MEDIA_CURL_PRIVATE_REQUEST_P_H_INCLUDED
#define ZYPP_NG_MEDIA_CURL_PRIVATE_REQUEST_P_H_INCLUDED

#include <zypp-core/CheckSum.h>
#include <zypp-core/ng/base/private/base_p.h>
#include <zypp-curl/ng/network/request.h>
#include <zypp-media/MediaException>
#include <zypp-core/ng/base/Timer>
#include <zypp-core/base/Regex.h>
#include <curl/curl.h>
#include <array>
#include <memory>
#include <zypp-core/Digest.h>
#include <zypp-core/AutoDispose.h>

#include <boost/optional.hpp>
#include <variant>

namespace zyppng {

  class NetworkRequestPrivate : public BasePrivate, public CurlMultiPartDataReceiver
  {
    ZYPP_DECLARE_PUBLIC(NetworkRequest)
  public:
    enum class ProtocolMode{
      Default, //< use this mode if no special checks are required in header or write callbacks
      HTTP    //< this mode is used for HTTP and HTTPS downloads
    } _protocolMode = ProtocolMode::Default;

    NetworkRequestPrivate(Url &&url, zypp::Pathname &&targetFile, NetworkRequest::FileMode fMode, NetworkRequest &p );
    ~NetworkRequestPrivate() override;

    bool initialize( std::string &errBuf );

    bool setupHandle ( std::string &errBuf );

    bool assertOutputFile ();

    /*!
     * \internal
     * Called by the dispatcher if we report a error. If this function returns
     * true we are qeued again and reinitialize is called
     */
    bool canRecover () const;

    /*!
     * Prepares the request before it is queued again
     * currently this is used only for range batching but could be used to
     * recover from other types of errors too
     */
    bool prepareToContinue ( std::string &errBuf  );

    /*!
     * \internal
     * This will return true if the download needs to be queued again to
     * continue downloading more stuff.
     */
    bool hasMoreWork() const;

    /*!
     * \internal
     * Prepares the request to be started, or restarted for a range batch request.
     */
    void aboutToStart ( );

    /*!
     * \internal
     * Tells the request it was just removed from the curl multi handle and is about
     * to be finalized. This function is called before \ref hasPendingRanges and \ref setResult
     */
    void dequeueNotify();

    void setResult ( NetworkRequestError &&err );
    void reset ();
    void resetActivityTimer ();

    void onActivityTimeout (Timer &);

    std::string errorMessage () const;


    std::array<char, CURL_ERROR_SIZE+1> _errorBuf; //provide a buffer for a nicely formatted error for CURL

    template<typename T>
    void setCurlOption ( CURLoption opt, T data )
    {
      auto ret = curl_easy_setopt( _easyHandle, opt, data );
      if ( ret != 0 ) {
        ZYPP_THROW( zypp::media::MediaCurlSetOptException( _url, _errorBuf.data() ) );
      }
    }

    Url                                 _url;        //file URL
    zypp::Pathname                      _targetFile; //target file
    TransferSettings                    _settings;
    NetworkRequest::Options             _options;
    zypp::ByteCount                     _expectedFileSize; // the file size as expected by the user code
    std::vector<NetworkRequest::Range>  _requestedRanges; ///< the requested ranges that need to be downloaded

    struct FileVerifyInfo {
      zypp::Digest _fileDigest;
      zypp::CheckSum _fileChecksum;
    };
    std::optional<FileVerifyInfo>       _fileVerification; ///< The digest for the full file

    NetworkRequest::FileMode            _fMode = NetworkRequest::WriteExclusive;
    NetworkRequest::Priority            _priority = NetworkRequest::Normal;

    std::string _lastRedirect;	///< to log/report redirections
    zypp::Pathname _currentCookieFile = "/var/lib/YaST2/cookies";

    void *_easyHandle = nullptr; // the easy handle that controlling this request
    NetworkRequestDispatcher *_dispatcher = nullptr; // the parent downloader owning this request

    //signals
    Signal< void ( NetworkRequest &req )> _sigStarted;
    Signal< void ( NetworkRequest &req, zypp::ByteCount count )> _sigBytesDownloaded;
    Signal< void ( NetworkRequest &req, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow )> _sigProgress;
    Signal< void ( NetworkRequest &req, const NetworkRequestError &err )> _sigFinished;

    static int curlProgressCallback ( void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow );

    size_t headerfunction ( char *ptr, size_t bytes ) override;
    size_t writefunction  (char *ptr, std::optional<off_t> offset, size_t bytes ) override;
    void notifyErrorCodeChanged () override;

    std::unique_ptr< curl_slist, decltype (&curl_slist_free_all) > _headers;

    struct pending_t;
    struct running_t;
    struct prepareNextRangeBatch_t;

    struct pending_t {
      pending_t(){}
      std::unique_ptr<CurlMultiPartHandler> _partialHelper = {};
    };

    struct prepareNextRangeBatch_t
    {
      prepareNextRangeBatch_t( running_t &&prevState );
      zypp::AutoFILE _outFile;     //the file we are writing to
      off_t _downloaded       = 0; //downloaded bytes
      std::unique_ptr<CurlMultiPartHandler> _partialHelper = {};
    };

    struct running_t  {
      running_t( pending_t &&prevState );
      running_t( prepareNextRangeBatch_t &&prevState );

      Timer::Ptr _activityTimer = Timer::create();

      zypp::AutoFILE _outFile;
      std::unique_ptr<CurlMultiPartHandler> _partialHelper = {};

      // handle the case when cancel() is called from a slot to the progress signal
      bool _isInCallback          = false;

      // used to handle cancel() when emitting the progress signal, or when we explicitely trigger
      // a error during callbacks.
      std::optional<NetworkRequestError> _cachedResult;

      off_t _lastProgressNow = -1; // last value returned from CURL, lets only send signals if we get actual updates
      off_t _downloaded = 0; //downloaded bytes
      off_t _currentFileOffset = 0;

      zypp::ByteCount _contentLenght; // the content length as reported by the server
    };

    struct finished_t {
      off_t               _downloaded = 0; //downloaded bytes
      zypp::ByteCount     _contentLenght = 0; // the content length as reported by the server
      NetworkRequestError _result; // the overall result of the download
    };

    std::variant< pending_t, running_t, prepareNextRangeBatch_t, finished_t > _runningMode = pending_t();
  };
}

#endif
