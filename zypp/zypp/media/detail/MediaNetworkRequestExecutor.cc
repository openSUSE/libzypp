#include "MediaNetworkRequestExecutor.h"
#include "DownloadProgressTracker.h"

#include <zypp-core/AutoDispose.h>
#include <zypp-core/ng/base/eventloop.h>
#include <zypp-core/ng/base/private/threaddata_p.h>
#include <zypp-curl/ng/network/networkrequestdispatcher.h>
#include <zypp-curl/ng/network/request.h>
#include <zypp-media/mediaexception.h>

namespace zypp::internal {

  MediaNetworkRequestExecutor::MediaNetworkRequestExecutor()
    : _evDispatcher( zyppng::ThreadData::current().ensureDispatcher() )
    , _nwDispatcher( std::make_shared<zyppng::NetworkRequestDispatcher>() )
  {

  }

  void MediaNetworkRequestExecutor::executeRequest( zyppng::NetworkRequestRef &req, callback::SendReport<media::DownloadProgressReport> *report )
  {
    auto loop = zyppng::EventLoop::create();

    _nwDispatcher->run();

    bool firstAuth = true;
    bool retry = true;
    int  maxTries = req->transferSettings().maxSilentTries();

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

      retry = false; // normally we don't retry
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
            excp = ZYPP_EXCPT_PTR( zypp::media::MediaFileNotFoundException( req->url() ) );
            break;
          }
          case zyppng::NetworkRequestError::Unauthorized:
          case zyppng::NetworkRequestError::AuthFailed: {

            //in case we got a auth hint from the server the error object will contain it
            std::string authHint = error.extraInfoValue("authHint", std::string());

            bool canContinue = false;
            _sigAuthRequired.emit( req->url(), req->transferSettings(), authHint, firstAuth, canContinue );
            if ( canContinue ) {
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
          if ( !retry && ( maxTries - 1 ) > 0 ) {
            maxTries--;
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
}
