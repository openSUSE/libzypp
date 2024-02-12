#ifndef ZYPPNG_CURLMULTIPARTHANDLER_H
#define ZYPPNG_CURLMULTIPARTHANDLER_H

#include <zypp-core/zyppng/base/Base>
#include <zypp-core/zyppng/core/ByteArray>
#include <zypp-core/Digest.h>
#include <zypp-core/zyppng/pipelines/Expected>
#include <zypp-curl/ng/network/NetworkRequestError>

#include <optional>
#include <any>

namespace zyppng {

  /*!
   * Data receiver interface for the \ref CurlMultiPartHandler class.
   */
  class CurlMultiPartDataReceiver {
  public:
    virtual ~CurlMultiPartDataReceiver() = default;

    /*!
     * Called for all received header data, after it was processed by the \ref CurlMultiPartHandler.
     */
    virtual size_t headerfunction ( char *ptr, size_t bytes ) = 0;

    /*!
     * Data callback func, this is called whenever there is actual data to be written to the file.
     * If \a offset is set, usually when starting to write a new range, it means to continue to write on the current file pointer position, otherwise
     * seek to the given one.
     */
    virtual size_t writefunction  ( char *ptr, std::optional<off_t> offset, size_t bytes ) = 0;

    /*!
     * Called everytime a new range is about to be written, returning false from the
     * function will immediately cancel the request and not write anything to the file.
     *
     * @param range The index of the range that is to be started
     * @param cancelReason Set to indicate why the request was cancelled.
     */
    virtual bool beginRange       ( off_t range, std::string &cancelReason ) { return true; };

    /*!
     * Called everytime a range was finished, returning false from the function will cancel the request.
     *
     * \note Normally \ref CurlMultiPartHandler will try to finish all ranges before failing even if one of them
     *       can not be validated. If the code should cancel early do it via returning false here.
     *
     * @param range The index of the range that was finished
     * @param validated Indicates of the range data could be validated against its given checksum
     * @param cancelReason Set to indicate why the request was cancelled.
     */
    virtual bool finishedRange    ( off_t range, bool validated, std::string &cancelReason ) { return true; };

    /*!
     * Called everytime the error code changes, this is just to notify that a error was set
     */
    virtual void notifyErrorCodeChanged () {};
  };

  /*!
   * \brief The CurlMultiPartHandler class
   *
   * multirange support for HTTP requests (https://tools.ietf.org/html/rfc7233), if not operating
   * on a HTTP/HTTPS URL this will batch multirange requests one by one, so maybe don't use it there
   *
   */
  class CurlMultiPartHandler : public Base
  {
    public:

      enum class ProtocolMode{
        Basic, //< use this mode if no special checks are required in header or write callbacks
        HTTP     //< this mode is used for HTTP and HTTPS downloads
      };

      // when requesting ranges from the server, we need to make sure not to request
      // too many at the same time. Instead we batch our requests and reuse the open
      // connection until we have the full file.
      // However different server have different maximum nr of ranges, so we start with
      // a high number and decrease until we find a rangecount that works
      constexpr static unsigned _rangeAttempt[] = {
        255,
        127,
        63,
        15,
        5,
        1
      };

      constexpr static unsigned _rangeAttemptSize = ( sizeof( _rangeAttempt ) / sizeof(unsigned) );

      enum State {
        Pending,    //< waiting to be dispatched
        Running,    //< currently running
        Finished,   //< finished successfully
        Error,      //< Error, use error function to figure out the issue
      };

      using CheckSumBytes = UByteArray;
      using Code = NetworkRequestError::Type;

      struct Range {
        size_t start = 0;
        size_t len = 0;
        size_t bytesWritten = 0;
        std::optional<zypp::Digest> _digest = {}; //< zypp::Digest that is updated when data is written, can be used to validate the file contents with a checksum

        /**
        * Enables automated checking of downloaded contents against a checksum.
        * Only makes a difference if \ref _digest is initialized too
        *
        * \note expects checksum in byte NOT in string format
        */
        CheckSumBytes _checksum;
        std::optional<size_t> _relevantDigestLen; //< If this is initialized , it defines how many bytes of the resulting checkum are compared
        std::optional<size_t> _chksumPad;   //< If initialized we need to pad the digest with zeros before calculating the final checksum
        std::any userData; //< Custom data the user can associate with the Range

        State _rangeState = State::Pending; //< Flag to know if this range has been already requested and if the request was successful

        void restart();
        Range clone() const;

        static Range make ( size_t start, size_t len = 0, std::optional<zypp::Digest> &&digest = {}, CheckSumBytes &&expectedChkSum = CheckSumBytes(), std::any &&userData = std::any(), std::optional<size_t> digestCompareLen = {}, std::optional<size_t> _dataBlockPadding = {} );
      };

      CurlMultiPartHandler( ProtocolMode mode, void *easyHandle, std::vector<Range> &ranges, CurlMultiPartDataReceiver &receiver );
      ~CurlMultiPartHandler() override;

      void *easyHandle() const;
      bool canRecover() const;
      bool hasMoreWork() const;

      bool hasError() const;

      Code lastError() const;
      const std::string &lastErrorMessage() const;

      bool validateRange(Range &rng);

      bool prepare( );
      bool prepareToContinue( );
      void finalize( );

      bool verifyData( );

      std::optional<size_t> reportedFileSize() const;
      std::optional<off_t>  currentRange() const;

  private:

      void setCode ( Code c, std::string msg, bool force = false );

      static size_t curl_hdrcallback ( char *ptr, size_t size, size_t nmemb, void *userdata );
      static size_t curl_wrtcallback ( char *ptr, size_t size, size_t nmemb, void *userdata );

      size_t hdrcallback ( char *ptr, size_t size, size_t nmemb );
      size_t wrtcallback ( char *ptr, size_t size, size_t nmemb );
      bool parseContentRangeHeader(const std::string_view &line, size_t &start, size_t &len, size_t &fileLen);
      bool parseContentTypeMultiRangeHeader(const std::string_view &line, std::string &boundary);
      bool checkIfRangeChkSumIsValid( Range &rng );
      void setRangeState ( Range &rng, State state );

      ProtocolMode _protocolMode = ProtocolMode::HTTP;
      void *_easyHandle = nullptr;
      CurlMultiPartDataReceiver &_receiver;

      Code _lastCode       = Code::NoError;
      std::string _lastErrorMsg;

      bool _allHeadersReceived = false; //< set to true once writefunc was called once e.g. all headers have been received
      bool _gotContentRangeInfo = false; //< Set to true if the server indicates ranges
      bool _isMuliPartResponse  = false; //< Set to true if the respone is in multipart form

      std::string _seperatorString; ///< The seperator string for multipart responses as defined in RFC 7233 Section 4.1
      std::vector<char> _rangePrefaceBuffer; ///< Here we buffer

      std::optional<off_t> _currentRange;
      std::optional<Range> _currentSrvRange;
      std::optional<size_t> _reportedFileSize; ///< Filesize as reported by the content range or byte range headers

      unsigned _rangeAttemptIdx = 0;
      std::vector<Range>  &_requestedRanges; ///< the requested ranges that need to be downloaded
  };

} // namespace zyppng

#endif // ZYPPNG_CURLMULTIPARTHANDLER_H
