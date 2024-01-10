#include "curlmultiparthandler.h"

#include <zypp-curl/ng/network/private/mediadebug_p.h>
#include <zypp-core/base/StringV.h>

#include <curl/curl.h>

namespace zyppng {

  namespace {

    class CurlMultiPartSetoptError : public zypp::Exception
    {
    public:
      CurlMultiPartSetoptError ( CURLcode code ) : _code(code){};
      CURLcode code() const;
    private:
      CURLcode _code;
    };

    CURLcode CurlMultiPartSetoptError::code() const {
      return _code;
    }

    class CurlMultInitRangeError : public zypp::Exception
    {
    public:
      CurlMultInitRangeError ( std::string what ) : zypp::Exception( std::move(what) ){}
    };

  }

  size_t CurlMultiPartHandler::curl_hdrcallback(char *ptr, size_t size, size_t nmemb, void *userdata)
  {
    if ( !userdata )
      return 0;

    CurlMultiPartHandler *that = reinterpret_cast<CurlMultiPartHandler *>( userdata );
    return that->hdrcallback( ptr, size, nmemb );
  }

  size_t CurlMultiPartHandler::curl_wrtcallback(char *ptr, size_t size, size_t nmemb, void *userdata)
  {
    if ( !userdata )
      return 0;

    CurlMultiPartHandler *that = reinterpret_cast<CurlMultiPartHandler *>( userdata );
    return that->wrtcallback( ptr, size, nmemb );
  }

  void CurlMultiPartHandler::Range::restart()
  {
    _rangeState  = CurlMultiPartHandler::Pending;
    bytesWritten = 0;
    if ( _digest ) _digest->reset();
  }

  CurlMultiPartHandler::Range CurlMultiPartHandler::Range::clone() const
  {
    return Range {
      .start = start,
      .len = len,
      .bytesWritten = 0,
      ._digest = _digest ? _digest->clone() : std::optional<zypp::Digest>{},
      ._checksum = _checksum,
      ._relevantDigestLen = _relevantDigestLen,
      ._chksumPad = _chksumPad,
      .userData = userData,
      ._rangeState = _rangeState
    };
  }

  CurlMultiPartHandler::Range CurlMultiPartHandler::Range::make(size_t start, size_t len, std::optional<zypp::Digest> &&digest, CheckSumBytes &&expectedChkSum, std::any &&userData, std::optional<size_t> digestCompareLen, std::optional<size_t> dataBlockPadding)
  {
    return Range {
      .start = start,
      .len   = len,
      .bytesWritten = 0,
      ._digest   = std::move( digest ),
      ._checksum = std::move( expectedChkSum ),
      ._relevantDigestLen = std::move( digestCompareLen ),
      ._chksumPad  = std::move( dataBlockPadding ),
      .userData = std::move( userData ),
      ._rangeState = State::Pending
    };
  }

  CurlMultiPartHandler::CurlMultiPartHandler(ProtocolMode mode, void *easyHandle, std::vector<Range> &ranges, CurlMultiPartDataReceiver &receiver)
    : _protocolMode( mode )
    , _easyHandle( easyHandle )
    , _receiver( receiver )
    , _requestedRanges( ranges )
  {
    // non http can only do range by range
    if ( _protocolMode == ProtocolMode::Basic ) {
      WAR << "!!!! Downloading ranges without HTTP might be slow !!!!" << std::endl;
      _rangeAttemptIdx = _rangeAttemptSize - 1;
    }
  }

  CurlMultiPartHandler::~CurlMultiPartHandler()
  {
    if ( _easyHandle ) {
      curl_easy_setopt( _easyHandle, CURLOPT_HEADERFUNCTION, nullptr );
      curl_easy_setopt( _easyHandle, CURLOPT_HEADERDATA, nullptr );
      curl_easy_setopt( _easyHandle, CURLOPT_WRITEFUNCTION, nullptr );
      curl_easy_setopt( _easyHandle, CURLOPT_WRITEDATA, nullptr );
    }
  }

  size_t CurlMultiPartHandler::hdrcallback(char *ptr, size_t size, size_t nmemb)
  {
    // it is valid to call this function with no data to read, just call the given handler
    // or return ok
    if ( size * nmemb == 0)
      return _receiver.headerfunction( ptr, size * nmemb );

    if ( _protocolMode == ProtocolMode::HTTP ) {

      std::string_view hdr( ptr, size*nmemb );

      hdr.remove_prefix( std::min( hdr.find_first_not_of(" \t\r\n"), hdr.size() ) );
      const auto lastNonWhitespace = hdr.find_last_not_of(" \t\r\n");
      if ( lastNonWhitespace != hdr.npos )
        hdr.remove_suffix( hdr.size() - (lastNonWhitespace + 1) );
      else
        hdr = std::string_view();

      // we just received whitespaces, wait for more
      if ( !hdr.size() ) {
        return ( size * nmemb );
      }

      if ( zypp::strv::hasPrefixCI( hdr, "HTTP/" ) ) {

        long statuscode = 0;
        (void)curl_easy_getinfo( _easyHandle, CURLINFO_RESPONSE_CODE, &statuscode);

        const auto &doRangeFail = [&](){
          WAR << _easyHandle << " " << "Range FAIL, trying with a smaller batch" << std::endl;

          setCode ( Code::RangeFail, "Expected range status code 206, but got none." );

          // reset all ranges we requested to pending, we never got the data for them
          std::for_each( _requestedRanges.begin (), _requestedRanges.end(), []( auto &range ) {
            if ( range._rangeState == Running )
              range._rangeState = Pending;
          });
          return 0;
        };

        // ignore other status codes, maybe we are redirected etc.
        if ( ( statuscode >= 200 && statuscode <= 299 && statuscode != 206 )
              || statuscode == 416 ) {
            return doRangeFail();
        }

      } else if ( zypp::strv::hasPrefixCI( hdr, "Content-Range:") ) {
        Range r;

        size_t fileLen = 0;
        if ( !parseContentRangeHeader( hdr, r.start, r.len, fileLen ) ) {
          //@TODO shouldn't we map this to a extra error? After all this is not our fault
          setCode( Code::InternalError, "Invalid Content-Range header format." );
          return 0;
        }

        if ( !_reportedFileSize ) {
          WAR << "Setting request filesize to " << fileLen << std::endl;
          _reportedFileSize = fileLen;
        }

        DBG << _easyHandle << " " << "Got content range :" << r.start << " len " << r.len << std::endl;
        _gotContentRangeInfo = true;
        _currentSrvRange = std::move(r);

      } else if ( zypp::strv::hasPrefixCI( hdr, "Content-Type:") ) {
        std::string sep;
        if ( parseContentTypeMultiRangeHeader( hdr, sep ) ) {
          _gotContentRangeInfo = true;
          _isMuliPartResponse  = true;
          _seperatorString = "--"+sep;
        }
      }
    }

    return _receiver.headerfunction( ptr, size * nmemb );
  }

  size_t CurlMultiPartHandler::wrtcallback(char *ptr, size_t size, size_t nmemb)
  {
    const auto max = ( size * nmemb );

    //it is valid to call this function with no data to write, just call the given handler
    if ( max == 0)
      return _receiver.writefunction( ptr, {}, max );

    // try to consume all bytes that have been given to us
    size_t bytesConsumedSoFar = 0;
    while ( bytesConsumedSoFar != max ) {

      std::optional<off_t> seekTo;

      // this is called after all headers have been processed
      if ( !_allHeadersReceived ) {
        _allHeadersReceived = true;

        // no ranges at all, this is a error
        if (  _protocolMode == ProtocolMode::HTTP && !_gotContentRangeInfo ) {
          //we got a invalid response, the status code pointed to being partial ( 206 ) but we got no range definition
          setCode( Code::ServerReturnedError, "Invalid data from server, range respone was announced but there was no range definiton." );
          return 0;
        }
      }

      if (  _protocolMode == ProtocolMode::HTTP && _currentSrvRange && !_currentRange  ) {
        /*
           this branch is responsible to find a range that overlaps the current server range, so we know which of our ranges to fill
           Entering here means we are in one of two situations:

           1) we just have finished writing a requested range but
              still have not completely consumed a range that we have received from the server.
              Since HTTP std allows the server to coalesce requested ranges in order to optimize downloads
              we need to find the best match ( because the current offset might not even be in our requested ranges )

           2) we just parsed a Content-Length header and start a new block
         */

        std::optional<uint> foundRange;
        const size_t beginSrvRange = _currentSrvRange->start + _currentSrvRange->bytesWritten;
        const size_t endSrvRange = beginSrvRange + (_currentSrvRange->len - _currentSrvRange->bytesWritten);
        auto currDist  = ULONG_MAX;
        for ( uint i = 0; i < _requestedRanges.size(); i++ ) {
          const auto &currR = _requestedRanges[i];

          // do not allow double ranges
          if ( currR._rangeState == Finished || currR._rangeState == Error )
            continue;

          // check if the range was already written
          if ( currR.len && currR.len == currR.bytesWritten )
            continue;

          const auto currRBegin = currR.start + currR.bytesWritten;
          if ( !( beginSrvRange <= currRBegin && endSrvRange >= currRBegin ) )
            continue;

          // calculate the distance of the current ranges offset+data written to the range we got back from the server
          const auto newDist   = currRBegin - beginSrvRange;

          // exact match
          if ( currRBegin == beginSrvRange && currR.len == _currentSrvRange->len ) {
            foundRange = i;
            break;
          }

          if ( !foundRange ) {
            foundRange = i;
            currDist = newDist;
          } else {
            //pick the range with the closest distance
            if ( newDist < currDist ) {
              foundRange = i;
              currDist = newDist;
            }
          }
        }

        if ( !foundRange ) {
          // @TODO shouldn't we simply consume the rest of the range here and see if future data will contain a matchable range again?
          setCode( Code::InternalError, "Unable to find a matching range for data returned by the server.");
          return 0;
        }

        //set the found range as the current one
        _currentRange = *foundRange;

        //continue writing where we stopped
        seekTo = _requestedRanges[*foundRange].start + _requestedRanges[*foundRange].bytesWritten;

        //if we skip bytes we need to advance our written bytecount
        const auto skipBytes = *seekTo - beginSrvRange;
        bytesConsumedSoFar += skipBytes;
        _currentSrvRange->bytesWritten += skipBytes;

        std::string errBuf = "Receiver cancelled starting the current range.";
        if ( !_receiver.beginRange (*_currentRange, errBuf) ) {
          setCode( Code::InternalError, "Receiver cancelled starting the current range.");
          return 0;
        }

      } else if ( _protocolMode != ProtocolMode::HTTP && !_currentRange ) {
        // if we are not running in HTTP mode we can only request a single range, that means we get our data
        // in one continous stream. Since our
        // ranges are ordered by start, we just pick the first one that is marked as running
        if ( !_currentRange ) {
          const auto i = std::find_if( _requestedRanges.begin (), _requestedRanges.end(), []( const Range &r){ return r._rangeState == Running; });
          if ( i == _requestedRanges.end() ) {
            setCode( Code::InternalError, "Received data but no range was marked as requested." );
            return 0;
          }

          _currentRange = std::distance( _requestedRanges.begin(), i );

          //continue writing where we stopped
          seekTo = _requestedRanges[*_currentRange].start + _requestedRanges[*_currentRange].bytesWritten;
        }
      }

      if ( _currentRange ) {
        /*
         * We have data to write and know the target range
         */

        // make sure we do not read over the current server range
        auto availableData = max - bytesConsumedSoFar;
        if ( _currentSrvRange ) {
          availableData = std::min( availableData, _currentSrvRange->len - _currentSrvRange->bytesWritten );
        }

        auto &rng = _requestedRanges[*_currentRange];

        // do only write what we need until the range is full
        const auto bytesToWrite = rng.len > 0 ? std::min( rng.len - rng.bytesWritten, availableData ) : availableData;

        auto written = _receiver.writefunction( ptr + bytesConsumedSoFar, seekTo, bytesToWrite );
        if ( written <= 0 )
          return 0;

        if ( rng._digest && rng._checksum.size() ) {
          if ( !rng._digest->update( ptr + bytesConsumedSoFar, written ) )
            return 0;
        }

        rng.bytesWritten += written;
        if ( _currentSrvRange ) _currentSrvRange->bytesWritten += written;

        // range is done
        if ( rng.len > 0 && rng.bytesWritten >= rng.len ) {
          std::string errBuf = "Receiver cancelled after finishing the current range.";
          bool rngIsValid = validateRange( rng );
          if ( !_receiver.finishedRange (*_currentRange, rngIsValid, errBuf) ) {
            setCode( Code::InternalError, "Receiver cancelled starting the current range.");
            return 0;
          }
          _currentRange.reset();
        }

        if ( _currentSrvRange && _currentSrvRange->len > 0 && _currentSrvRange->bytesWritten >= _currentSrvRange->len ) {
          _currentSrvRange.reset();
          // we ran out of data in the current chunk, reset the target range if it is not a open range as well, because next data will be
          // a chunk header again
          if ( _isMuliPartResponse )
            _currentRange.reset();
        }

        bytesConsumedSoFar += written;
      }

      if ( bytesConsumedSoFar == max )
        return max;

      if ( _protocolMode == ProtocolMode::HTTP && !_currentRange && !_currentSrvRange ) {
        /*
           This branch is reponsible to parse the multibyte response we got from the
           server and parse the next target range from it
         */

        std::string_view incoming( ptr + bytesConsumedSoFar, max - bytesConsumedSoFar );
        auto hdrEnd = incoming.find("\r\n\r\n");
        if ( hdrEnd == incoming.npos ) {
          //no header end in the data yet, push to buffer and return
           _rangePrefaceBuffer.insert( _rangePrefaceBuffer.end(), incoming.begin(), incoming.end() );
           return max;
        }

        //append the data of the current header to the buffer and parse it
        _rangePrefaceBuffer.insert( _rangePrefaceBuffer.end(), incoming.begin(), incoming.begin() + ( hdrEnd + 4 )  );
        bytesConsumedSoFar += ( hdrEnd + 4 ); //header data plus header end

        std::string_view data( _rangePrefaceBuffer.data(), _rangePrefaceBuffer.size() );
        auto sepStrIndex = data.find( _seperatorString );
        if ( sepStrIndex == data.npos ) {
          setCode( Code::InternalError, "Invalid multirange header format, seperator string missing." );
          return 0;
        }

        auto startOfHeader = sepStrIndex + _seperatorString.length();
        std::vector<std::string_view> lines;
        zypp::strv::split( data.substr( startOfHeader ), "\r\n", zypp::strv::Trim::trim, [&]( std::string_view strv ) { lines.push_back(strv); } );
        for ( const auto &hdrLine : lines ) {
          if ( zypp::strv::hasPrefixCI(hdrLine, "Content-Range:") ) {
            size_t fileLen = 0;
            Range r;
            //if we can not parse the header the message must be broken
            if(! parseContentRangeHeader( hdrLine, r.start, r.len, fileLen ) ) {
              setCode( Code::InternalError, "Invalid Content-Range header format." );
              return 0;
            }
            if ( !_reportedFileSize ) {
              _reportedFileSize = fileLen;
            }
            _currentSrvRange = std::move(r);
            break;
          }
        }

        if ( !_currentSrvRange ){
          setCode( Code::InternalError, "Could not read a server range from the response." );
          return 0;
        }

        //clear the buffer again
        _rangePrefaceBuffer.clear();
      }
    }

    return bytesConsumedSoFar;
  }

  void *CurlMultiPartHandler::easyHandle() const
  {
    return _easyHandle;
  }

  bool CurlMultiPartHandler::canRecover() const
  {
    // We can recover from RangeFail errors if we have more batch sizes to try
    // we never auto downgrade to last range set ( which is 1 ) because in that case
    // just downloading the full file is usually faster.
    if ( _lastCode == Code::RangeFail )
      return ( _rangeAttemptIdx + 1 < ( _rangeAttemptSize - 1 )  ) && hasMoreWork();
    return false;
  }

  bool CurlMultiPartHandler::hasMoreWork() const
  {
    // check if we have ranges that have never been requested
    return std::any_of( _requestedRanges.begin(), _requestedRanges.end(), []( const auto &range ){ return range._rangeState == Pending; });
  }

  bool CurlMultiPartHandler::hasError() const
  {
    return _lastCode != Code::NoError;
  }

  CurlMultiPartHandler::Code CurlMultiPartHandler::lastError() const
  {
    return _lastCode;
  }

  const std::string &CurlMultiPartHandler::lastErrorMessage() const
  {
    return _lastErrorMsg;
  }

  bool CurlMultiPartHandler::prepareToContinue( )
  {
    if ( hasMoreWork() ) {
      // go to the next range batch level if we are restarted due to a failed range request
      if ( _lastCode == Code::RangeFail ) {
        if ( _rangeAttemptIdx + 1 >= _rangeAttemptSize ) {
          setCode ( Code::RangeFail, "No more range batch sizes available", true );
          return false;
        }
        _rangeAttemptIdx++;
      }
      return true;
    }

    setCode ( Code::NoError, "Request has no more work", true );
    return false;

  }

  void CurlMultiPartHandler::finalize()
  {
    // if we still have a current range set it valid by checking the checksum
    if ( _currentRange ) {
      auto &currR = _requestedRanges[*_currentRange];
      std::string errBuf;
      bool rngIsValid = validateRange( currR );
      _receiver.finishedRange (*_currentRange, rngIsValid, errBuf);
      _currentRange.reset();
    }
  }

  bool CurlMultiPartHandler::verifyData()
  {
    finalize();

    for ( auto &r : _requestedRanges ) {
      if ( r._rangeState != CurlMultiPartHandler::Finished ) {
        if ( r.len > 0 && r.bytesWritten != r.len )
          setCode( Code::MissingData, (zypp::str::Format("Did not receive all requested data from the server ( off: %1%, req: %2%, recv: %3% ).") % r.start % r.len % r.bytesWritten ) );
        else if ( r._digest && r._checksum.size() && !checkIfRangeChkSumIsValid(r) )  {
          setCode( Code::InvalidChecksum, (zypp::str::Format("Invalid checksum %1%, expected checksum %2%") % r._digest->digest() % zypp::Digest::digestVectorToString( r._checksum ) ) );
        } else {
          setCode( Code::InternalError, (zypp::str::Format("Download of block failed.") ) );
        }
        //we only report the first error
        break;
      }
    }
    return ( _lastCode == Code::NoError );
  }

  bool CurlMultiPartHandler::prepare( )
  {
    _lastCode = Code::NoError;
    _lastErrorMsg.clear();
    _seperatorString.clear();
    _currentSrvRange.reset();
    _reportedFileSize.reset();
    _gotContentRangeInfo = false;
    _allHeadersReceived  = false;
    _isMuliPartResponse  = false;

    if ( _requestedRanges.size() == 0 ) {
      setCode( Code::InternalError, "Calling the CurlMultiPartHandler::prepare function without a range to download is not supported.");
      return false;
    }

    const auto setCurlOption = [&]( CURLoption opt, auto &&data )
    {
      auto ret = curl_easy_setopt( _easyHandle, opt, data );
      if ( ret != 0 ) {
        throw CurlMultiPartSetoptError(ret);
      }
    };

    try {
      setCurlOption( CURLOPT_HEADERFUNCTION, CurlMultiPartHandler::curl_hdrcallback );
      setCurlOption( CURLOPT_HEADERDATA, this );
      setCurlOption( CURLOPT_WRITEFUNCTION, CurlMultiPartHandler::curl_wrtcallback );
      setCurlOption( CURLOPT_WRITEDATA, this );

      std::string rangeDesc;
      uint rangesAdded = 0;
      auto maxRanges = _rangeAttempt[_rangeAttemptIdx];

      // helper function to build up the request string for the range
      auto addRangeString = [ &rangeDesc, &rangesAdded ]( const std::pair<size_t, size_t> &range ) {
        std::string rangeD = zypp::str::form("%llu-", static_cast<unsigned long long>( range.first ) );
        if( range.second > 0 )
          rangeD.append( zypp::str::form( "%llu", static_cast<unsigned long long>( range.second ) ) );

        if ( rangeDesc.size() )
          rangeDesc.append(",").append( rangeD );
        else
          rangeDesc = std::move( rangeD );

        rangesAdded++;
      };

      std::optional<std::pair<size_t, size_t>> currentZippedRange;
      bool closedRange = true;
      for ( auto &range : _requestedRanges ) {

        if ( range._rangeState != Pending )
          continue;

        //reset the download results
        range.bytesWritten = 0;

        //when we have a open range in the list of ranges we will get from start of range to end of file,
        //all following ranges would never be marked as valid, so we have to fail early
        if ( !closedRange )
          throw CurlMultInitRangeError("It is not supported to request more ranges after a open range.");

        const auto rangeEnd = range.len > 0 ? range.start + range.len - 1 : 0;
        closedRange = (rangeEnd > 0);

        bool added = false;

        // we try to compress the requested ranges into as big chunks as possible for the request,
        // when receiving we still track the original ranges so we can collect and test their checksums
        if ( !currentZippedRange ) {
          added = true;
          currentZippedRange = std::make_pair( range.start, rangeEnd );
        } else {
          //range is directly consecutive to the previous range
          if ( currentZippedRange->second + 1 == range.start ) {
            added = true;
            currentZippedRange->second = rangeEnd;
          } else {
            //this range does not directly follow the previous one, we build the string and start a new one
            if ( rangesAdded +1 >= maxRanges ) break;
            added = true;
            addRangeString( *currentZippedRange );
            currentZippedRange = std::make_pair( range.start, rangeEnd );
          }
        }

        // remember range was already requested
        if ( added ) {
          setRangeState( range, Running );
          range.bytesWritten = 0;
          if ( range._digest )
            range._digest->reset();
        }

        if ( rangesAdded >= maxRanges ) {
          MIL << _easyHandle << " " << "Reached max nr of ranges (" << maxRanges << "), batching the request to not break the server" << std::endl;
          break;
        }
      }

      // add the last range too
      if ( currentZippedRange )
        addRangeString( *currentZippedRange );

      MIL << _easyHandle << " " << "Requesting Ranges: " << rangeDesc << std::endl;

      setCurlOption( CURLOPT_RANGE, rangeDesc.c_str() );

    } catch( const CurlMultiPartSetoptError &err ) {
      setCode( Code::InternalError, "" );
    } catch( const CurlMultInitRangeError &err ) {
      setCode( Code::InternalError, err.asUserString() );
    } catch( const zypp::Exception &err ) {
      setCode( Code::InternalError, err.asUserString() );
    } catch( const std::exception &err ) {
      setCode( Code::InternalError, zypp::str::asString(err.what()) );
    }
    return ( _lastCode == Code::NoError );
  }

  void CurlMultiPartHandler::setCode(Code c, std::string msg , bool force)
  {
    // never overwrite a error, this is reset when we restart
    if ( _lastCode != Code::NoError && !force )
      return;

    _lastCode = c;
    _lastErrorMsg = msg;
  }

  bool CurlMultiPartHandler::parseContentRangeHeader( const std::string_view &line, size_t &start, size_t &len, size_t &fileLen )
  {
    //content-range: bytes 10485760-19147879/19147880
    static const zypp::str::regex regex("^Content-Range:[[:space:]]+bytes[[:space:]]+([0-9]+)-([0-9]+)\\/([0-9]+)$", zypp::str::regex::rxdefault | zypp::str::regex::icase );

    zypp::str::smatch what;
    if( !zypp::str::regex_match( std::string(line), what, regex ) || what.size() != 4 ) {
      DBG << _easyHandle << " " << "Invalid Content-Range Header format: '" << std::string(line) << std::endl;
      return false;
    }

    size_t s = zypp::str::strtonum<size_t>( what[1]);
    size_t e = zypp::str::strtonum<size_t>( what[2]);
    fileLen  = zypp::str::strtonum<size_t>( what[3]);
    start = std::move(s);
    len   = ( e - s ) + 1;
    return true;
  }

  bool CurlMultiPartHandler::parseContentTypeMultiRangeHeader( const std::string_view &line, std::string &boundary )
  {
    static const zypp::str::regex regex("^Content-Type:[[:space:]]+multipart\\/byteranges;[[:space:]]+boundary=(.*)$", zypp::str::regex::rxdefault | zypp::str::regex::icase );

    zypp::str::smatch what;
    if( zypp::str::regex_match( std::string(line), what, regex )  ) {
      if ( what.size() >= 2 ) {
        boundary = what[1];
        return true;
      }
    }
    return false;
  }

  bool CurlMultiPartHandler::validateRange( Range &rng )
  {
    if ( rng._digest && rng._checksum.size() ) {
      if ( ( rng.len == 0 || rng.bytesWritten == rng.len ) && checkIfRangeChkSumIsValid(rng) )
        setRangeState(rng, Finished);
      else
        setRangeState(rng, Error);
    } else {
      if ( rng.len == 0 ? true : rng.bytesWritten == rng.len )
        setRangeState(rng, Finished);
      else
        setRangeState(rng, Error);
    }
    return ( rng._rangeState == Finished );
  }

  bool CurlMultiPartHandler::checkIfRangeChkSumIsValid ( Range &rng )
  {
    if ( rng._digest && rng._checksum.size() ) {
      auto bytesHashed = rng._digest->bytesHashed ();
      if ( rng._chksumPad && *rng._chksumPad > bytesHashed ) {
        MIL_MEDIA << _easyHandle << " " << "Padding the digest to required block size" << std::endl;
        zypp::ByteArray padding( *rng._chksumPad - bytesHashed, '\0' );
        rng._digest->update( padding.data(), padding.size() );
      }
      auto digVec = rng._digest->digestVector();
      if ( rng._relevantDigestLen ) {
        digVec.resize( *rng._relevantDigestLen );
      }
      return ( digVec == rng._checksum );
    }

    // no checksum required
    return true;
  }

  void CurlMultiPartHandler::setRangeState( Range &rng, State state )
  {
    if ( rng._rangeState != state ) {
      rng._rangeState = state;
    }
  }

  std::optional<off_t> CurlMultiPartHandler::currentRange() const
  {
    return _currentRange;
  }

  std::optional<size_t> CurlMultiPartHandler::reportedFileSize() const
  {
    return _reportedFileSize;
  }

} // namespace zyppng
