#include "MediaNetworkRequestExecutor.h"
#include "DownloadProgressTracker.h"

#include <zypp-core/AutoDispose.h>
#include <zypp-core/ng/base/eventloop.h>
#include <zypp-core/ng/base/SocketNotifier>
#include <zypp-core/ng/base/private/threaddata_p.h>
#include <zypp-core/ng/base/private/linuxhelpers_p.h>
#include <zypp-curl/ng/network/networkrequestdispatcher.h>
#include <zypp-curl/ng/network/request.h>
#include <zypp-media/mediaexception.h>
#include <zypp-core/base/UserRequestException>
#include <zypp/zypp_detail/ZYppImpl.h> // for shutdownSignalFd

#include <poll.h>

namespace zypp::internal {

  namespace {

    // Used both as the cancel reason and as the thrown exception message.
    constexpr const char * kShutdownMessage = "Shutdown signal received.";

    // True iff a shutdown was requested and not yet cleared. The pipe is readable while
    // the pending byte is in it. The byte is never drained here, so the request stays
    // aborted until the application calls ZYpp::clearShutdownSignal(); this matches the
    // legacy zypp_poll() backend.
    bool isShutdownPending( int shutdownFd )
    {
      if ( shutdownFd == -1 )
        return false;
      pollfd pfd { shutdownFd, POLLIN, 0 };
      return zyppng::eintrSafeCall( ::poll, &pfd, 1, 0 ) > 0 && ( pfd.revents & POLLIN );
    }

  } // anonymous namespace

  MediaNetworkRequestExecutor::MediaNetworkRequestExecutor()
    : _evDispatcher( zyppng::ThreadData::current().ensureDispatcher() )
    , _nwDispatcher( std::make_shared<zyppng::NetworkRequestDispatcher>() )
  {

  }

  void MediaNetworkRequestExecutor::executeRequest( zyppng::NetworkRequestRef &req, callback::SendReport<media::DownloadProgressReport> *report )
  {
    const int shutdownFd = zypp_detail::shutdownSignalFd();

    // A pending shutdown aborts before anything is enqueued, so nothing is added to the
    // dispatcher at all. No progress report is emitted here on purpose: the report only
    // sees a start() once the request is actually enqueued.
    if ( isShutdownPending( shutdownFd ) )
      ZYPP_THROW( UserRequestException( UserRequestException::ABORT, kShutdownMessage ) );

    auto loop = zyppng::EventLoop::create();

    _nwDispatcher->run();

    // Watch the shutdown fd while the request is in flight. The byte is deliberately not
    // drained, so the request stays aborted until the application clears the signal.
    bool aborted = false;
    zyppng::SocketNotifier::Ptr shutdownWatch;
    if ( shutdownFd != -1 ) {
      shutdownWatch = zyppng::SocketNotifier::create( shutdownFd, zyppng::SocketNotifier::Read, false );
      zyppng::SocketNotifier *watch = shutdownWatch.get();
      watch->sigActivated().connect( [ &, watch ]( const zyppng::SocketNotifier &, int ) {
        // Level triggered and never drained: disable it first, otherwise it fires again
        // immediately.
        watch->setEnabled( false );
        aborted = true;
        _nwDispatcher->cancelAll( kShutdownMessage );
      } );
      watch->setEnabled( true );
    }
    zypp_defer { shutdownWatch.reset(); };

    bool started = false;
    bool firstAuth = true;
    bool retry = true;
    int  maxTries = req->transferSettings().maxSilentTries();

    // A shutdown is terminal: it is never retried and never surfaced as a media error.
    // A UserRequestException is deliberately NOT a MediaException, so it is not caught by
    // the mirror-retry (MediaCurl2) or media-change (MediaSetAccess) handlers and rides
    // the existing terminal path in RepoProvideFile/PackageProvider, exactly like the
    // legacy zypp_poll() backend.
    const auto throwOnShutdown = [&]() {
      if ( started )
        (*report)->finish( req->url(), zypp::media::DownloadProgressReport::ERROR, kShutdownMessage );
      ZYPP_THROW( UserRequestException( UserRequestException::ABORT, kShutdownMessage ) );
    };

    while ( retry ) {
      // A shutdown that arrived while the event loop was not running (between the
      // pre-check and the enqueue, or between retries) is still terminal; it must not
      // degrade into a retryable media error.
      if ( isShutdownPending( shutdownFd ) )
        throwOnShutdown();

      std::optional<internal::ProgressTracker> progTracker;

      std::vector<zyppng::connection> signalConnections {
        req->sigStarted().connect( [&]( zyppng::NetworkRequest &req ){
          if ( !report) return;
          (*report)->start( req.url(), req.targetFilePath() );
          started = true;
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
      // The request might already be finished at this point, e.g. if setting up the
      // curl handle failed. Calling loop->quit() before loop->run() has no effect,
      // so only run the loop if the request is still pending or running.
      if ( _nwDispatcher->count () != 0 )
        loop->run();

      // The notifier only fires while the loop is running, so this catches an in-flight
      // abort. It is checked before hasError() so a shutdown can not be turned into a
      // MediaException.
      if ( aborted )
        throwOnShutdown();

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
