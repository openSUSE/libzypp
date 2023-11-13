/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/

#include <zypp-curl/ng/network/private/downloader_p.h>
#include <zypp-curl/ng/network/private/mediadebug_p.h>
#include <zypp-curl/ng/network/private/request_p.h>
#include <zypp-curl/ng/network/private/networkrequesterror_p.h>

#include "metalinkinfo_p.h"

namespace zyppng {

  namespace  {
  }

  DlMetaLinkInfoState::DlMetaLinkInfoState(DownloadPrivate &parent)
    : BasicDownloaderStateBase( parent )
  {
    MIL << "Downloading metalink/zsync on " << parent._spec.url() << std::endl;
  }

  DlMetaLinkInfoState::DlMetaLinkInfoState(std::shared_ptr<Request> &&prevRequest, DownloadPrivate &parent)
    : BasicDownloaderStateBase( std::move(prevRequest), parent )
  {
    MIL << "Downloading metalink/zsync on " << parent._spec.url() << std::endl;
  }

  std::shared_ptr<FinishedState> DlMetaLinkInfoState::transitionToFinished()
  {
    MIL << "Downloading on " << stateMachine()._spec.url() << " transition to final state. " << std::endl;
    return std::make_shared<FinishedState>( std::move(_error), stateMachine() );
  }

  std::shared_ptr<PrepareMultiState> DlMetaLinkInfoState::transitionToPrepareMulti()
  {
    _request->disconnectSignals();
    auto prepareMode = ( _detectedMetaType == MetaDataType::MetaLink ? PrepareMultiState::Metalink : PrepareMultiState::Zsync );
    auto nState = std::make_shared<PrepareMultiState>( std::move( _request ), prepareMode, stateMachine() );
    _request = nullptr;
    return nState;
  }

  bool DlMetaLinkInfoState::initializeRequest(std::shared_ptr<Request> &r )
  {
    MIL << "Requesting Metadata info from server!" << std::endl;
    r->transferSettings().addHeader("Accept: */*, application/x-zsync, application/metalink+xml, application/metalink4+xml");
    return BasicDownloaderStateBase::initializeRequest(r);
  }

  void DlMetaLinkInfoState::gotFinished()
  {
    // some proxies do not store the content type, so also look at the file to find
    // out if we received a metalink (bnc#649925)
    if ( _detectedMetaType == MetaDataType::None )
      _detectedMetaType = zypp::media::looks_like_meta_file( _request->targetFilePath() );
    if ( _detectedMetaType == MetaDataType::None ) {
      // Move to finished state
      MIL << "Downloading on " << stateMachine()._spec.url() << " was successful, no metalink/zsync data. " << std::endl;
      return BasicDownloaderStateBase::gotFinished();
    }

    auto &sm = stateMachine();
    if ( sm._stopOnMetalink ) {
      MIL << "Stopping after receiving MetaData as requested" << std::endl;
      sm._stoppedOnMetalink = true;
      return BasicDownloaderStateBase::gotFinished();
    }

    // Move to Prepare Multi state
    if ( _detectedMetaType == MetaDataType::Zsync )
      MIL << "Downloading on " << sm._spec.url() << " returned a Zsync file " << std::endl;
    else
      MIL << "Downloading on " << sm._spec.url() << " returned a Metalink file" << std::endl;
    _sigGotMetadata.emit();
  }

  void DlMetaLinkInfoState::handleRequestProgress(NetworkRequest &req, off_t dltotal, off_t dlnow)
  {
    auto &sm = stateMachine();

    if ( _detectedMetaType == MetaDataType::None && dlnow < zypp::media::minMetalinkProbeSize ) {
      // can't tell yet, ...
      return sm._sigAlive.emit( *sm.z_func(), dlnow );
    }

    if ( _detectedMetaType == MetaDataType::None ) {
      std::string cType = req.contentType();
      if ( cType.find("application/x-zsync") == 0 )
        _detectedMetaType = MetaDataType::Zsync;
      else if ( cType.find("application/metalink+xml") == 0 || cType.find("application/metalink4+xml") == 0 )
        _detectedMetaType = MetaDataType::MetaLink;
    }

    if ( _detectedMetaType == MetaDataType::None ) {
      _detectedMetaType = zypp::media::looks_like_meta_data( req.peekData( 0, zypp::media::minMetalinkProbeSize ) );
    }

    if ( _detectedMetaType != MetaDataType::None ) {
      // this is a metalink file change the expected filesize
      if ( zypp::ByteCount( 2, zypp::ByteCount::MB) < req.downloadedByteCount() ) {
        WAR << "Metadata file exceeds 2MB in filesize, aborting."<<std::endl;
        sm._requestDispatcher->cancel( req, NetworkRequestErrorPrivate::customError( NetworkRequestError::ExceededMaxLen ) );
        return;
      }

      return sm._sigAlive.emit( *sm.z_func(), dlnow );

    } else {
      // still no metalink, we assume a normal download, not perfect though
      if ( !_fallbackMilWritten ) {
        _fallbackMilWritten = true;
        MIL << "No Metalink file detected after " << zypp::media::minMetalinkProbeSize << ", falling back to normal progress updates" << std::endl;
      }
      return BasicDownloaderStateBase::handleRequestProgress( req, dltotal, dlnow );
    }
  }
}
