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

#include <zypp/base/Logger.h>
#include <zypp/ExternalProgram.h>
#include <zypp/base/String.h>
#include <zypp/base/Gettext.h>
#include <zypp-core/parser/Sysconfig>

#include <zypp/media/MediaCurl2.h>

#include <zypp-core/zyppng/base/private/linuxhelpers_p.h>
#include <zypp-core/base/userrequestexception.h>
#include <zypp-curl/ProxyInfo>
#include <zypp-curl/CurlConfig>
#include <zypp-curl/private/curlhelper_p.h>
#include <zypp/Target.h>
#include <zypp/ZYppFactory.h>
#include <zypp/ZConfig.h>
#include <zypp/zypp_detail/ZYppImpl.h> // for zypp_poll

#include <zypp-curl/ng/network/networkrequestdispatcher.h>
#include <zypp-curl/ng/network/request.h>

#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <dirent.h>
#include <unistd.h>
#include <glib.h>

#include "detail/MediaNetworkRequestExecutor.h"
#include "detail/OptionalDownloadProgressReport.h"
#include <zypp-curl/ng/network/private/mediadebug_p.h>

#ifdef ENABLE_ZCHUNK_COMPRESSION
#include <zypp-curl/ng/network/zckhelper.h>
#endif


/*!
 * TODOS:
 *  - Add overall timeout for the request
 *  - IFMODSINCE
 */

using std::endl;

using namespace internal;
using namespace zypp::base;

namespace zypp {

  namespace media {

    MediaCurl2::MediaCurl2(const MediaUrl &url_r, const std::vector<MediaUrl> &mirrors_r,
                           const Pathname & attach_point_hint_r )
      : MediaNetworkCommonHandler( url_r, mirrors_r, attach_point_hint_r,
                                   "/", // urlpath at attachpoint
                                   true ) // does_download
      , _executor( std::make_shared<internal::MediaNetworkRequestExecutor>() )
    {

      MIL << "MediaCurl2::MediaCurl2(" << url_r.url() << ", " << attach_point_hint_r << ")" << endl;

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
              << " is not useable for " << url_r.url().getScheme() << endl;
          setAttachPoint("", true);
        }
        else if( atest != NULL)
          ::rmdir(atest);

        if( atemp != NULL)
          ::free(atemp);
      }
    }

    ///////////////////////////////////////////////////////////////////

    void MediaCurl2::checkProtocol(const Url &url) const
    {
      if ( !zyppng::NetworkRequestDispatcher::supportsProtocol ( url ) )
      {
        std::string msg("Unsupported protocol '");
        msg += url.getScheme();
        msg += "'";
        ZYPP_THROW(MediaBadUrlException(_url.url(), msg));
      }
    }

    void MediaCurl2::disconnectFrom()
    {
      // clear effective settings
      _mirrorSettings.clear();
    }

    ///////////////////////////////////////////////////////////////////

    void MediaCurl2::releaseFrom( const std::string & ejectDev )
    {
      disconnect();
    }

    ///////////////////////////////////////////////////////////////////

    void MediaCurl2::getFileCopy( const OnMediaLocation & srcFile , const Pathname & target ) const
    {

      const auto &filename = srcFile.filename();

      // Optional files will send no report until data are actually received (we know it exists).
      OptionalDownloadProgressReport reportfilter( srcFile.optional() );
      callback::SendReport<DownloadProgressReport> report;

      for ( int mirr = 0; mirr < _urls.size(); mirr++ ) {

        MIL << "Trying to fetch file " << srcFile << " with mirror " << _urls[mirr].url() << std::endl;
        Url fileurl(getFileUrl(mirr, filename));

        try
        {
          if(!_urls[mirr].url().isValid())
            ZYPP_THROW(MediaBadUrlException(_urls[mirr].url()));

          if(_urls[mirr].url().getHost().empty())
            ZYPP_THROW(MediaBadUrlEmptyHostException(_urls[mirr].url()));


          Pathname dest = target.absolutename();
          if( assert_dir( dest.dirname() ) ) {
            DBG << "assert_dir " << dest.dirname() << " failed" << endl;
            ZYPP_THROW( MediaSystemException(fileurl, "System error on " + dest.dirname().asString()) );
          }

          ManagedFile destNew { target.extend( ".new.zypp.XXXXXX" ) }; {
            AutoFREE<char> buf { ::strdup( (*destNew).c_str() ) };
            if( ! buf ) {
              ERR << "out of memory for temp file name" << endl;
              ZYPP_THROW(MediaSystemException(fileurl, "out of memory for temp file name"));
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
          DBG << "URL: " << fileurl.asString() << endl;

          // Use URL without options and without username and passwd
          // (some proxies dislike them in the URL).
          // Curl seems to need the just scheme, hostname and a path;
          // the rest was already passed as curl options (in attachTo).
          Url curlUrl( clearQueryString(fileurl) );

          RequestData r;
          r._mirrorIdx = mirr;
          r._req = std::make_shared<zyppng::NetworkRequest>( curlUrl, destNew, zyppng::NetworkRequest::WriteShared /*do not truncate*/ );
          r._req->transferSettings() = _mirrorSettings[mirr];
          r._req->setExpectedFileSize ( srcFile.downloadSize () );

          bool done = false;
    #ifdef ENABLE_ZCHUNK_COMPRESSION
          done = const_cast<MediaCurl2*>(this)->tryZchunk(r, srcFile, destNew, report);
    #endif
          if ( !done ) {
            r._req->resetRequestRanges();
            const_cast<MediaCurl2 *>(this)->executeRequest ( r, &report );
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

          break;  // success!
        }
        catch (MediaException & excpt_r)
        {
          // check if we can retry on the next mirror
          if( !canTryNextMirror ( excpt_r ) || ( mirr+1 >= _urls.size() ) ) {
            // rewrite the exception to contain the correct pathname and url
            // the executeRequest implementation just emits the full Url in the exception
            if ( typeid(excpt_r) == typeid( MediaFileNotFoundException ) ) {
              ZYPP_CAUGHT(excpt_r);
              ZYPP_THROW( MediaFileNotFoundException( url(), filename ) );
            }
            ZYPP_RETHROW(excpt_r);
          }
          ZYPP_CAUGHT(excpt_r);
          continue;
        }
      }
    }

    bool MediaCurl2::getDoesFileExist( const Pathname & filename ) const
    {
      DBG << filename.asString() << endl;

      std::exception_ptr lastErr;
      for ( int mirr = 0; mirr < _urls.size(); mirr++ ) {
        if( !_urls[mirr].url().isValid() )
          ZYPP_THROW(MediaBadUrlException(_urls[mirr].url()));

        if( _urls[mirr].url().getHost().empty() )
          ZYPP_THROW(MediaBadUrlEmptyHostException(_urls[mirr].url()));

        Url url(getFileUrl(mirr, filename));

        DBG << "URL: " << url.asString() << endl;

        // Use URL without options and without username and passwd
        // (some proxies dislike them in the URL).
        // Curl seems to need the just scheme, hostname and a path;
        // the rest was already passed as curl options (in attachTo).
        Url curlUrl( clearQueryString(url) );

        RequestData r;
        r._mirrorIdx = mirr;
        r._req = std::make_shared<zyppng::NetworkRequest>( curlUrl, "/dev/null" );
        r._req->setOptions ( zyppng::NetworkRequest::HeadRequest ); // just check for existance
        r._req->transferSettings() = _mirrorSettings[mirr];

        // as we are not having user interaction, the user can't cancel
        // the file existence checking, a callback or timeout return code
        // will be always a timeout.
        try {
          const_cast<MediaCurl2*>(this)->executeRequest ( r );

        } catch ( const MediaException &e ) {
          // check if we can retry on the next mirror
          if( !canTryNextMirror ( e ) ) {
            ZYPP_RETHROW(e);
          }
          lastErr = ZYPP_FWD_CURRENT_EXCPT();
          continue;
        }
        // exists
        return ( !r._req->hasError() );
      }

      if ( lastErr ) {
        try {
          ZYPP_RETHROW (lastErr);
        } catch ( const MediaFileNotFoundException &e ) {
          // if the file did not exist then we can return false
          return false;
        }
      }
      // we probably never had mirrors, otherwise we either had an error or a success.
      return false;
    }

    bool MediaCurl2::tryZchunk( RequestData &reqData, const OnMediaLocation &srcFile, const Pathname &target, callback::SendReport<DownloadProgressReport> &report )
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

        reqData._req->addRequestRange( 0, srcFile.headerSize(), std::move(digest), sum );
        executeRequest ( reqData, nullptr );

        reqData._req->resetRequestRanges();

        auto res = zyppng::ZckHelper::prepareZck( srcFile.deltafile(), target, srcFile.downloadSize() );
        switch(res._code) {
          case zyppng::ZckHelper::PrepareResult::Error: {
            ERR << "Failed to setup zchunk because of: " << res._message << std::endl;
            return false;
          }
          case zyppng::ZckHelper::PrepareResult::NothingToDo:
            return true; // already done
          case zyppng::ZckHelper::PrepareResult::ExceedsMaxLen:
            ZYPP_THROW( MediaFileSizeExceededException( reqData._req->url(), srcFile.downloadSize(), res._message ));
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
            reqData._req->addRequestRange( block._start, block._len, std::move(dig), block._checksum, {}, block._relevantDigestLen, block._chksumPad );
          }
        };

        executeRequest ( reqData, &report );

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

    void MediaCurl2::executeRequest(  MediaCurl2::RequestData &reqData , callback::SendReport<DownloadProgressReport> *report )
    {
      const auto &authCb = [&]( const zypp::Url &, TransferSettings &settings, const std::string & availAuthTypes, bool firstTry, bool &canContinue ) {
        if ( authenticate( _urls[reqData._mirrorIdx].url(), _mirrorSettings[reqData._mirrorIdx], availAuthTypes, firstTry ) ) {
          settings = _mirrorSettings[reqData._mirrorIdx];
          canContinue = true;
          return;
        }
        canContinue = false;
      };

      auto conn = _executor->sigAuthRequired().connect (authCb);
      zypp_defer {
        conn.disconnect ();
      };

      _executor->executeRequest ( reqData._req, report );
    }
  } // namespace media
} // namespace zypp
//
