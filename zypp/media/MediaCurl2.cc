/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaCurl2.cc
 *
*/

#include <iostream>
#include <chrono>
#include <list>

#include <zypp/base/Logger.h>
#include <zypp/ExternalProgram.h>
#include <zypp/base/String.h>
#include <zypp/base/Gettext.h>
#include <zypp-core/parser/Sysconfig>
#include <zypp/base/Gettext.h>

#include <zypp/media/MediaCurl2.h>

#include <zypp-core/zyppng/base/private/linuxhelpers_p.h>
#include <zypp-core/base/userrequestexception.h>
#include <zypp-curl/ProxyInfo>
#include <zypp-curl/auth/CurlAuthData>
#include <zypp-media/auth/CredentialManager>
#include <zypp-curl/CurlConfig>
#include <zypp-curl/private/curlhelper_p.h>
#include <zypp/Target.h>
#include <zypp/ZYppFactory.h>
#include <zypp/ZConfig.h>
#include <zypp/zypp_detail/ZYppImpl.h> // for zypp_poll

#include <zypp-core/zyppng/base/eventloop.h>
#include <zypp-core/zyppng/base/private/threaddata_p.h>
#include <zypp-curl/ng/network/networkrequestdispatcher.h>
#include <zypp-curl/ng/network/request.h>

#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <dirent.h>
#include <unistd.h>
#include <glib.h>

#include "detail/OptionalDownloadProgressReport.h"
#include "zypp-curl/ng/network/private/mediadebug_p.h"

#ifdef ENABLE_ZCHUNK_COMPRESSION
#include <zypp-curl/ng/network/zckhelper.h>
#endif


/*!
 * TODOS:
 *  - Add overall timeout for the request
 *  - IFMODSINCE
 */

using std::endl;

namespace internal {
  using namespace zypp;

  struct ProgressTracker {

    using clock = std::chrono::steady_clock;

    std::optional<clock::time_point> _timeStart; ///< Start total stats
    std::optional<clock::time_point> _timeLast;	 ///< Start last period(~1sec)

    double _dnlTotal	= 0.0;	///< Bytes to download or 0 if unknown
    double _dnlLast	= 0.0;	  ///< Bytes downloaded at period start
    double _dnlNow	= 0.0;	  ///< Bytes downloaded now

    int    _dnlPercent= 0;	///< Percent completed or 0 if _dnlTotal is unknown

    double _drateTotal= 0.0;	///< Download rate so far
    double _drateLast	= 0.0;	///< Download rate in last period

    void updateStats( double dltotal = 0.0, double dlnow = 0.0 )
    {
      clock::time_point now = clock::now();

      if ( !_timeStart )
        _timeStart = _timeLast = now;

      // If called without args (0.0), recompute based on the last values seen
      if ( dltotal && dltotal != _dnlTotal )
        _dnlTotal = dltotal;

      if ( dlnow && dlnow != _dnlNow ) {
        _dnlNow = dlnow;
      }

      // percentage:
      if ( _dnlTotal )
        _dnlPercent = int(_dnlNow * 100 / _dnlTotal);

      // download rates:
      _drateTotal = _dnlNow / std::max( std::chrono::duration_cast<std::chrono::seconds>(now - *_timeStart).count(), int64_t(1) );

      if ( _timeLast < now )
      {
        _drateLast = (_dnlNow - _dnlLast) / int( std::chrono::duration_cast<std::chrono::seconds>(now - *_timeLast).count() );
        // start new period
        _timeLast  = now;
        _dnlLast   = _dnlNow;
      }
      else if ( _timeStart == _timeLast )
        _drateLast = _drateTotal;
    }
  };
}

using namespace internal;
using namespace zypp::base;

namespace zypp {

  namespace media {

    MediaCurl2::MediaCurl2( const Url &      url_r,
                            const Pathname & attach_point_hint_r )
      : MediaNetworkCommonHandler( url_r, attach_point_hint_r,
                                   "/", // urlpath at attachpoint
                                   true ) // does_download
      , _evDispatcher( zyppng::ThreadData::current().ensureDispatcher() )
      , _nwDispatcher( std::make_shared<zyppng::NetworkRequestDispatcher>() )
    {

      MIL << "MediaCurl2::MediaCurl2(" << url_r << ", " << attach_point_hint_r << ")" << endl;

      if( !attachPoint().empty())
      {
        PathInfo ainfo(attachPoint());
        Pathname apath(attachPoint() + "XXXXXX");
        char    *atemp = ::strdup( apath.asString().c_str());
        char    *atest = NULL;
        if( !ainfo.isDir() || !ainfo.userMayRWX() ||
            atemp == NULL || (atest=::mkdtemp(atemp)) == NULL)
        {
          WAR << "attach point " << ainfo.path()
              << " is not useable for " << url_r.getScheme() << endl;
          setAttachPoint("", true);
        }
        else if( atest != NULL)
          ::rmdir(atest);

        if( atemp != NULL)
          ::free(atemp);
      }

      _nwDispatcher->setAgentString ( str::asString( agentString () ) );
      _nwDispatcher->setHostSpecificHeader ("download.opensuse.org", "X-ZYpp-DistributionFlavor", str::asString(distributionFlavorHeader()) );
      _nwDispatcher->setHostSpecificHeader ("download.opensuse.org", "X-ZYpp-AnonymousId", str::asString(anonymousIdHeader()) );
    }

    Url MediaCurl2::clearQueryString(const Url &url) const
    {
      return internal::clearQueryString(url);
    }

    ///////////////////////////////////////////////////////////////////

    void MediaCurl2::checkProtocol(const Url &url) const
    {
      if ( !zyppng::NetworkRequestDispatcher::supportsProtocol ( url ) )
      {
        std::string msg("Unsupported protocol '");
        msg += url.getScheme();
        msg += "'";
        ZYPP_THROW(MediaBadUrlException(_url, msg));
      }
    }

    void MediaCurl2::setupEasy()
    {
      // fill some settings from url query parameters
      try
      {
        _effectiveSettings = _settings;
        fillSettingsFromUrl(_url, _effectiveSettings);
      }
      catch ( const MediaException &e )
      {
        disconnectFrom();
        ZYPP_RETHROW(e);
      }
      // if the proxy was not set (or explicitly unset) by url, then look...
      if ( _effectiveSettings.proxy().empty() )
      {
        // ...at the system proxy settings
        fillSettingsSystemProxy(_url, _effectiveSettings);
      }

      /* Fixes bsc#1174011 "auth=basic ignored in some cases"
       * We should proactively add the password to the request if basic auth is configured
       * and a password is available in the credentials but not in the URL.
       *
       * We will be a bit paranoid here and require that the URL has a user embedded, otherwise we go the default route
       * and ask the server first about the auth method
       */
      if ( _effectiveSettings.authType() == "basic"
           && _effectiveSettings.username().size()
           && !_effectiveSettings.password().size() ) {

        CredentialManager cm(CredManagerOptions(ZConfig::instance().repoManagerRoot()));
        const auto cred = cm.getCred( _url );
        if ( cred && cred->valid() ) {
          if ( !_effectiveSettings.username().size() )
            _effectiveSettings.setUsername(cred->username());
          _effectiveSettings.setPassword(cred->password());
        }
      }
    }

    void MediaCurl2::attachTo (bool next)
    {
      if ( next )
        ZYPP_THROW(MediaNotSupportedException(_url));

      if ( !_url.isValid() )
        ZYPP_THROW(MediaBadUrlException(_url));

      checkProtocol(_url);
      if( !isUseableAttachPoint( attachPoint() ) )
      {
        setAttachPoint( createAttachPoint(), true );
      }

      disconnectFrom(); // clean state if needed

      // here : setup TransferSettings
      setupEasy();

      // FIXME: need a derived class to propelly compare url's
      MediaSourceRef media( new MediaSource(_url.getScheme(), _url.asString()));
      setMediaSource(media);
    }

    bool
    MediaCurl2::checkAttachPoint(const Pathname &apoint) const
    {
      return MediaHandler::checkAttachPoint( apoint, true, true);
    }

    void MediaCurl2::disconnectFrom()
    {
      // clear effective settings
      _effectiveSettings = _settings;
    }

    ///////////////////////////////////////////////////////////////////

    void MediaCurl2::releaseFrom( const std::string & ejectDev )
    {
      disconnect();
    }

    ///////////////////////////////////////////////////////////////////

    void MediaCurl2::getFile( const OnMediaLocation &file ) const
    {
      // Use absolute file name to prevent access of files outside of the
      // hierarchy below the attach point.
      getFileCopy( file, localPath(file.filename()).absolutename() );
    }

    ///////////////////////////////////////////////////////////////////

    void MediaCurl2::getFileCopy( const OnMediaLocation & srcFile , const Pathname & target ) const
    {

      const auto &filename = srcFile.filename();

      // Optional files will send no report until data are actually received (we know it exists).
      OptionalDownloadProgressReport reportfilter( srcFile.optional() );
      callback::SendReport<DownloadProgressReport> report;

      Url fileurl(getFileUrl(filename));
      do
      {
        try
        {
          doGetFileCopy( srcFile, target, report );
          break;  // success!
        }
        // unexpected exception
        catch (MediaException & excpt_r)
        {
          media::DownloadProgressReport::Error reason = media::DownloadProgressReport::ERROR;
          if( typeid(excpt_r) == typeid( media::MediaFileNotFoundException )  ||
              typeid(excpt_r) == typeid( media::MediaNotAFileException ) )
          {
            reason = media::DownloadProgressReport::NOT_FOUND;
          }
          report->finish(fileurl, reason, excpt_r.asUserHistory());
          ZYPP_RETHROW(excpt_r);
        }
      }
      while ( true );
      report->finish(fileurl, zypp::media::DownloadProgressReport::NO_ERROR, "");
    }

    bool MediaCurl2::getDoesFileExist( const Pathname & filename ) const
    {
      DBG << filename.asString() << endl;

      if(!_url.isValid())
        ZYPP_THROW(MediaBadUrlException(_url));

      if(_url.getHost().empty())
        ZYPP_THROW(MediaBadUrlEmptyHostException(_url));

      Url url(getFileUrl(filename));

      DBG << "URL: " << url.asString() << endl;

      // Use URL without options and without username and passwd
      // (some proxies dislike them in the URL).
      // Curl seems to need the just scheme, hostname and a path;
      // the rest was already passed as curl options (in attachTo).
      Url curlUrl( clearQueryString(url) );

      auto req = std::make_shared<zyppng::NetworkRequest>( curlUrl, "/dev/null" );
      req->setOptions ( zyppng::NetworkRequest::HeadRequest ); // just check for existance

      // as we are not having user interaction, the user can't cancel
      // the file existence checking, a callback or timeout return code
      // will be always a timeout.
      try {
        const_cast<MediaCurl2*>(this)->executeRequest ( req );
      }
      catch ( const MediaFileNotFoundException &e ) {
        // if the file did not exist then we can return false
        return false;
      }
      catch ( const MediaException &e ) {
        // some error, we are not sure about file existence, rethrw
        ZYPP_RETHROW(e);
      }

      // exists
      return ( !req->hasError() );
    }

    ///////////////////////////////////////////////////////////////////

    void MediaCurl2::doGetFileCopy( const OnMediaLocation &srcFile , const Pathname & target, callback::SendReport<DownloadProgressReport> & report, RequestOptions options ) const
    {
      Pathname dest = target.absolutename();
      if( assert_dir( dest.dirname() ) ) {
        DBG << "assert_dir " << dest.dirname() << " failed" << endl;
        ZYPP_THROW( MediaSystemException(getFileUrl(srcFile.filename()), "System error on " + dest.dirname().asString()) );
      }

      ManagedFile destNew { target.extend( ".new.zypp.XXXXXX" ) }; {
        AutoFREE<char> buf { ::strdup( (*destNew).c_str() ) };
        if( ! buf ) {
          ERR << "out of memory for temp file name" << endl;
          ZYPP_THROW(MediaSystemException(getFileUrl(srcFile.filename()), "out of memory for temp file name"));
        }

        AutoFD tmp_fd { ::mkostemp( buf, O_CLOEXEC ) };
        if( tmp_fd == -1 ) {
          ERR << "mkstemp failed for file '" << destNew << "'" << endl;
          ZYPP_THROW(MediaWriteException(destNew));
        }
        destNew = ManagedFile( (*buf), filesystem::unlink );
      }

      DBG << "dest: " << dest << endl;
      DBG << "temp: " << destNew << endl;
#if 0
      Not implemented here yet because NetworkRequest can not do IFMODSINCE yet
      // set IFMODSINCE time condition (no download if not modified)
      if( PathInfo(target).isExist() && !(options & OPTION_NO_IFMODSINCE) )
      {
        curl_easy_setopt(_curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
        curl_easy_setopt(_curl, CURLOPT_TIMEVALUE, (long)PathInfo(target).mtime());
      }
      else
      {
        curl_easy_setopt(_curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
        curl_easy_setopt(_curl, CURLOPT_TIMEVALUE, 0L);
      }
#endif

      DBG << srcFile.filename().asString() << endl;

      if(!_url.isValid())
        ZYPP_THROW(MediaBadUrlException(_url));

      if(_url.getHost().empty())
        ZYPP_THROW(MediaBadUrlEmptyHostException(_url));

      Url url(getFileUrl(srcFile.filename()));

      DBG << "URL: " << url.asString() << endl;
      // Use URL without options and without username and passwd
      // (some proxies dislike them in the URL).
      // Curl seems to need the just scheme, hostname and a path;
      // the rest was already passed as curl options (in attachTo).
      Url curlUrl( clearQueryString(url) );

      auto req = std::make_shared<zyppng::NetworkRequest>( curlUrl, destNew, zyppng::NetworkRequest::WriteShared /*do not truncate*/ );
      req->setExpectedFileSize ( srcFile.downloadSize () );

      bool done = false;
#ifdef ENABLE_ZCHUNK_COMPRESSION
      done = const_cast<MediaCurl2*>(this)->tryZchunk(req, srcFile, destNew, report);
#endif
      if ( !done ) {
        req->resetRequestRanges();
        const_cast<MediaCurl2 *>(this)->executeRequest ( req, &report );
      }

#if 0
      Also disabled IFMODSINCE code, see above while not yet implemented here
    #if CURLVERSION_AT_LEAST(7,19,4)
          // bnc#692260: If the client sends a request with an If-Modified-Since header
          // with a future date for the server, the server may respond 200 sending a
          // zero size file.
          // curl-7.19.4 introduces CURLINFO_CONDITION_UNMET to check this condition.
          if ( ftell(file) == 0 && ret == 0 )
      {
        long httpReturnCode = 33;
        if ( curl_easy_getinfo( _curl, CURLINFO_RESPONSE_CODE, &httpReturnCode ) == CURLE_OK && httpReturnCode == 200 )
        {
          long conditionUnmet = 33;
          if ( curl_easy_getinfo( _curl, CURLINFO_CONDITION_UNMET, &conditionUnmet ) == CURLE_OK && conditionUnmet )
          {
            WAR << "TIMECONDITION unmet - retry without." << endl;
            curl_easy_setopt(_curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
            curl_easy_setopt(_curl, CURLOPT_TIMEVALUE, 0L);
            ret = executeCurl();
          }
        }
      }
#endif
#endif


      // apply umask
      if ( ::chmod( destNew->c_str(), filesystem::applyUmaskTo( 0644 ) ) )
      {
        ERR << "Failed to chmod file " << destNew << endl;
      }

      // move the temp file into dest
      if ( rename( destNew, dest ) != 0 ) {
        ERR << "Rename failed" << endl;
        ZYPP_THROW(MediaWriteException(dest));
      }
      destNew.resetDispose();	// no more need to unlink it

      DBG << "done: " << PathInfo(dest) << endl;
    }


    bool MediaCurl2::tryZchunk( zyppng::NetworkRequestRef req, const OnMediaLocation &srcFile, const Pathname &target, callback::SendReport<DownloadProgressReport> &report )
    {
#ifdef ENABLE_ZCHUNK_COMPRESSION

      // HERE add zchunk logic if required
      if ( !srcFile.deltafile().empty()
           && zyppng::ZckHelper::isZchunkFile (srcFile.deltafile ())
           && srcFile.headerSize () > 0 ) {

        // first fetch the zck header
        std::optional<zypp::Digest> digest;
        UByteArray sum;

        const auto &headerSum = srcFile.headerChecksum();
        if ( !headerSum.empty () ) {
          digest = zypp::Digest();
          if ( !digest->create( headerSum.type() ) ) {
            ERR << "Unknown header checksum type " << headerSum.type() << std::endl;
            return false;
          }
          sum = zypp::Digest::hexStringToUByteArray( headerSum.checksum() );
        }

        req->addRequestRange( 0, srcFile.headerSize(), std::move(digest), sum );
        executeRequest ( req, nullptr );

        req->resetRequestRanges();

        auto res = zyppng::ZckHelper::prepareZck( srcFile.deltafile(), target, srcFile.downloadSize() );
        switch(res._code) {
          case zyppng::ZckHelper::PrepareResult::Error: {
            ERR << "Failed to setup zchunk because of: " << res._message << std::endl;
            return false;
          }
          case zyppng::ZckHelper::PrepareResult::NothingToDo:
            return true; // already done
          case zyppng::ZckHelper::PrepareResult::ExceedsMaxLen:
            ZYPP_THROW( MediaFileSizeExceededException( req->url(), srcFile.downloadSize(), res._message ));
          case zyppng::ZckHelper::PrepareResult::Success:
            break;
        }

        for ( const auto &block : res._blocks ) {
          if ( block._checksum.size() && block._chksumtype.size() ) {
            std::optional<zypp::Digest> dig = zypp::Digest();
            if ( !dig->create( block._chksumtype ) ) {
              WAR_MEDIA << "Trying to create Digest with chksum type " << block._chksumtype << " failed " << std::endl;
              return false;
            }

            if ( zypp::env::ZYPP_MEDIA_CURL_DEBUG() > 3 )
              DBG_MEDIA << "Starting block " << block._start << " with checksum " << zypp::Digest::digestVectorToString( block._checksum ) << "." << std::endl;
            req->addRequestRange( block._start, block._len, std::move(dig), block._checksum, {}, block._relevantDigestLen, block._chksumPad );
          }
        };

        executeRequest ( req, &report );

        //we might have the file ready
        std::string err;
        if ( !zyppng::ZckHelper::validateZckFile( target, err) ) {
          ERR << "ZCK failed with error: " << err << std::endl;
          return false;
        }
        return true;
      }
#endif
      return false;
    }



    ///////////////////////////////////////////////////////////////////

    void MediaCurl2::getDir( const Pathname & dirname, bool recurse_r ) const
    {
      filesystem::DirContent content;
      getDirInfo( content, dirname, /*dots*/false );

      for ( filesystem::DirContent::const_iterator it = content.begin(); it != content.end(); ++it ) {
        Pathname filename = dirname + it->name;
        int res = 0;

        switch ( it->type ) {
          case filesystem::FT_NOT_AVAIL: // old directory.yast contains no typeinfo at all
          case filesystem::FT_FILE:
            getFile( OnMediaLocation( filename ) );
            break;
          case filesystem::FT_DIR: // newer directory.yast contain at least directory info
            if ( recurse_r ) {
              getDir( filename, recurse_r );
            } else {
              res = assert_dir( localPath( filename ) );
              if ( res ) {
                WAR << "Ignore error (" << res <<  ") on creating local directory '" << localPath( filename ) << "'" << endl;
              }
            }
            break;
          default:
            // don't provide devices, sockets, etc.
            break;
        }
      }
    }

    ///////////////////////////////////////////////////////////////////

    void MediaCurl2::getDirInfo( std::list<std::string> & retlist,
                                 const Pathname & dirname, bool dots ) const
    {
      getDirectoryYast( retlist, dirname, dots );
    }

    ///////////////////////////////////////////////////////////////////

    void MediaCurl2::getDirInfo( filesystem::DirContent & retlist,
                                 const Pathname & dirname, bool dots ) const
    {
      getDirectoryYast( retlist, dirname, dots );
    }

    void MediaCurl2::executeRequest( zyppng::NetworkRequestRef req , callback::SendReport<DownloadProgressReport> *report )
    {
      auto loop = zyppng::EventLoop::create();

      _nwDispatcher->run();

      bool firstAuth = true;
      bool retry = true;
      int  maxTries = _effectiveSettings.maxSilentTries();

      while ( retry ) {
        std::optional<internal::ProgressTracker> progTracker;

        std::vector<zyppng::connection> signalConnections {
          req->sigStarted().connect( [&]( zyppng::NetworkRequest &req ){
            if ( !report) return;
            (*report)->start( req.url(), req.targetFilePath() );
          }),
          req->sigProgress().connect( [&]( zyppng::NetworkRequest &req, off_t dlTotal, off_t dlNow, off_t, off_t ){
            if ( !report || !progTracker )
              return;

            progTracker->updateStats( dlTotal, dlNow );
            if ( !(*report)->progress( progTracker->_dnlPercent, req.url(), progTracker-> _drateTotal, progTracker->_drateLast ) )
              _nwDispatcher->cancel ( req );

          }),
          req->sigFinished().connect( [&]( zyppng::NetworkRequest &req, const zyppng::NetworkRequestError &err ) {
            loop->quit();
          })
        };

        // clean up slots for every loop
        zypp_defer {
          std::for_each( signalConnections.begin(), signalConnections.end(), []( auto &conn ) { conn.disconnect(); });
          signalConnections.clear();
        };

        if ( report ) {
          progTracker = internal::ProgressTracker();
        }

        maxTries--;
        retry = false; // normally we don't retry
        req->transferSettings() = _effectiveSettings; // use settings from MediaCurl
        _nwDispatcher->enqueue ( req );
        loop->run();

        // once the request is done there should be nothing there anymore
        if ( _nwDispatcher->count () != 0 ) {
          ZYPP_THROW( zypp::Exception("Unexpected request count after finishing MediaCurl2 request!") );
        }

        if ( req->hasError() ) {
          auto errCode = zypp::media::DownloadProgressReport::ERROR;
          std::exception_ptr excp;
          const auto &error = req->error();
          switch ( error.type() ) {
            case zyppng::NetworkRequestError::InternalError:
            case zyppng::NetworkRequestError::InvalidChecksum:
            case zyppng::NetworkRequestError::UnsupportedProtocol:
            case zyppng::NetworkRequestError::MalformedURL:
            case zyppng::NetworkRequestError::PeerCertificateInvalid:
            case zyppng::NetworkRequestError::ConnectionFailed:
            case zyppng::NetworkRequestError::ServerReturnedError:
            case zyppng::NetworkRequestError::MissingData:
            case zyppng::NetworkRequestError::RangeFail: {
              excp = ZYPP_EXCPT_PTR( zypp::media::MediaCurlException( req->url(), error.toString(), error.nativeErrorString() ) );
              break;
            }
            case zyppng::NetworkRequestError::Cancelled: {
              excp = ZYPP_EXCPT_PTR( zypp::media::MediaRequestCancelledException( error.toString() ) );
              break;
            }
            case zyppng::NetworkRequestError::ExceededMaxLen: {
              excp = ZYPP_EXCPT_PTR( zypp::media::MediaFileSizeExceededException( req->url(), req->expectedFileSize() ) );
              break;
            }
            case zyppng::NetworkRequestError::TemporaryProblem: {
              excp = ZYPP_EXCPT_PTR( zypp::media::MediaTemporaryProblemException( req->url(), error.toString() ) );
              break;
            }
            case zyppng::NetworkRequestError::Timeout: {
              excp = ZYPP_EXCPT_PTR( zypp::media::MediaTimeoutException( req->url(), error.toString() ) );
              break;
            }
            case zyppng::NetworkRequestError::Forbidden: {
              excp = ZYPP_EXCPT_PTR( zypp::media::MediaForbiddenException( req->url(), error.toString() ) );
              break;
            }
            case zyppng::NetworkRequestError::NotFound: {
              errCode = zypp::media::DownloadProgressReport::NOT_FOUND;

              //@BUG using getPathName() can result in wrong error messages
              excp = ZYPP_EXCPT_PTR( zypp::media::MediaFileNotFoundException( _url, req->url().getPathName() ) );
              break;
            }
            case zyppng::NetworkRequestError::Unauthorized:
            case zyppng::NetworkRequestError::AuthFailed: {

              //in case we got a auth hint from the server the error object will contain it
              std::string authHint = error.extraInfoValue("authHint", std::string());
              if ( authenticate( authHint, firstAuth ) ) {
                firstAuth = false;
                retry = true;
                continue;
              }

              errCode = zypp::media::DownloadProgressReport::ACCESS_DENIED;
              excp = ZYPP_EXCPT_PTR( zypp::media::MediaUnauthorizedException( req->url(), error.toString(), error.nativeErrorString(), "" ) );
              break;
            }
            case zyppng::NetworkRequestError::NoError:
              // should never happen
              DBG << "BUG: Download error flag is set , but Error code is NoError" << std::endl;
              break;
            case zyppng::NetworkRequestError::Http2Error:
            case zyppng::NetworkRequestError::Http2StreamError: {
              excp = ZYPP_EXCPT_PTR( zypp::media::MediaCurlException( req->url(), error.toString(), error.nativeErrorString() ) );
              break;
            }
          }

          if ( excp ) {
            if ( maxTries > 0 ) {
              retry = true;
              continue;
            }

            if ( report ) (*report)->finish( req->url(), errCode, error.toString() );
            std::rethrow_exception( excp );
          }
        }

      }

      if ( report ) (*report)->finish( req->url(), zypp::media::DownloadProgressReport::NO_ERROR, "" );
    }

    ///////////////////////////////////////////////////////////////////

    bool MediaCurl2::authenticate(const std::string & availAuthTypes, bool firstTry)
    {
      //! \todo need a way to pass different CredManagerOptions here
      CredentialManager cm(CredManagerOptions(ZConfig::instance().repoManagerRoot()));
      CurlAuthData_Ptr credentials;

      // get stored credentials
      AuthData_Ptr cmcred = cm.getCred(_url);

      if (cmcred && firstTry)
      {
        credentials.reset(new CurlAuthData(*cmcred));
        DBG << "got stored credentials:" << endl << *credentials << endl;
      }
      // if not found, ask user
      else
      {

        CurlAuthData_Ptr curlcred;
        curlcred.reset(new CurlAuthData());
        callback::SendReport<AuthenticationReport> auth_report;

        // preset the username if present in current url
        if (!_url.getUsername().empty() && firstTry)
          curlcred->setUsername(_url.getUsername());
        // if CM has found some credentials, preset the username from there
        else if (cmcred)
          curlcred->setUsername(cmcred->username());

        // indicate we have no good credentials from CM
        cmcred.reset();

        std::string prompt_msg = str::Format(_("Authentication required for '%s'")) % _url.asString();

        // set available authentication types from the exception
        // might be needed in prompt
        curlcred->setAuthType(availAuthTypes);

        // ask user
        if (auth_report->prompt(_url, prompt_msg, *curlcred))
        {
          DBG << "callback answer: retry" << endl
              << "CurlAuthData: " << *curlcred << endl;

          if (curlcred->valid())
          {
            credentials = curlcred;
            // if (credentials->username() != _url.getUsername())
            //   _url.setUsername(credentials->username());
            /**
           *  \todo find a way to save the url with changed username
           *  back to repoinfo or dont store urls with username
           *  (and either forbid more repos with the same url and different
           *  user, or return a set of credentials from CM and try them one
           *  by one)
           */
          }
        }
        else
        {
          DBG << "callback answer: cancel" << endl;
        }
      }

      // set username and password
      if (credentials)
      {
        _effectiveSettings.setUsername(credentials->username());
        _effectiveSettings.setPassword(credentials->password());

        // set available authentication types from the exception
        if (credentials->authType() == CURLAUTH_NONE)
          credentials->setAuthType(availAuthTypes);

        // set auth type (seems this must be set _after_ setting the userpwd)
        if (credentials->authType() != CURLAUTH_NONE) {
          _effectiveSettings.setAuthType(credentials->authTypeAsString());
        }

        if (!cmcred)
        {
          credentials->setUrl(_url);
          cm.addCred(*credentials);
          cm.save();
        }

        return true;
      }

      return false;
    }

  } // namespace media
} // namespace zypp
//
