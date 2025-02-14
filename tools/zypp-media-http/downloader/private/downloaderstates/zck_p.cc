/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/

#if ENABLE_ZCHUNK_COMPRESSION

#include <downloader/private/downloader_p.h>
#include <zypp-curl/ng/network/private/mediadebug_p.h>
#include <zypp-curl/ng/network/private/networkrequesterror_p.h>
#include <zypp-curl/ng/network/zckhelper.h>
#include <zypp-core/AutoDispose.h>

#include "zck_p.h"

#include <iostream>
#include <fstream>
#include <fcntl.h>

extern "C" {
#include <zck.h>
}

namespace zyppng {

  DLZckHeadState::DLZckHeadState( std::vector<Url> &&mirrors, std::shared_ptr<Request> &&oldReq, DownloadPrivate &parent )
    : BasicDownloaderStateBase( std::move(oldReq), parent )
  {
    _fileMirrors = std::move(mirrors);
    MIL << "About to enter DlZckHeadState for url " << parent._spec.url() << std::endl;
  }

  DLZckHeadState::DLZckHeadState( std::vector<Url> &&mirrors, DownloadPrivate &parent )
    : BasicDownloaderStateBase( parent )
  {
    _fileMirrors = std::move(mirrors);
    MIL << "About to enter DlZckHeadState for url " << parent._spec.url() << std::endl;
  }


  bool DLZckHeadState::initializeRequest(std::shared_ptr<Request> &r )
  {
    BasicDownloaderStateBase::initializeRequest( r );

    const auto &s = stateMachine()._spec;
    if ( s.headerSize() == 0 ) {
      ERR << "Downloading the zck header was requested, but headersize is zero." << std::endl;
      return false;
    }

    std::optional<zypp::Digest> digest;
    NetworkRequest::CheckSumBytes sum;

    const auto &headerSum = s.headerChecksum();
    if ( headerSum ) {
      digest = zypp::Digest();
      if ( !digest->create( headerSum->type() ) ) {
        ERR << "Unknown header checksum type " << headerSum->type() << std::endl;
        return false;
      }
      sum = zypp::Digest::hexStringToUByteArray( headerSum->checksum() );
    }

    r->addRequestRange( 0, s.headerSize(), std::move(digest), sum );
    return true;
  }

  void DLZckHeadState::gotFinished()
  {
    if ( ZckHelper::isZchunkFile( stateMachine()._spec.targetPath() ) )
      return BasicDownloaderStateBase::gotFinished();
    failed ( "Downloaded header is not a zchunk header");
  }

  std::shared_ptr<DLZckState> DLZckHeadState::transitionToDlZckState()
  {
    MIL_MEDIA << "Downloaded the header of size: " << _request->downloadedByteCount() << std::endl;
    return std::make_shared<DLZckState>( std::move(_fileMirrors), stateMachine() );
  }

  DLZckState::DLZckState(std::vector<Url> &&mirrors, DownloadPrivate &parent)
    : RangeDownloaderBaseState( std::move(mirrors), parent )
  {
    MIL_MEDIA << "About to enter DLZckState for url " << parent._spec.url() << std::endl;
  }

  void DLZckState::enter()
  {
    auto &sm = stateMachine();
    const auto &spec = sm._spec;

    // setup the base downloader
    _error = {};
    _ranges.clear();
    _failedRanges.clear();

    // @TODO get this from zchunk file?
    _fileSize = spec.expectedFileSize();

    auto prepareRes = ZckHelper::prepareZck ( spec.deltaFile (), spec.targetPath (), _fileSize );
    switch( prepareRes._code ) {
      case ZckHelper::PrepareResult::Error: {
        return setFailed ( std::move(prepareRes._message) );
      }
      case ZckHelper::PrepareResult::ExceedsMaxLen: {
        return setFailed( NetworkRequestErrorPrivate::customError(
                            NetworkRequestError::ExceededMaxLen,
                            std::move(prepareRes._message) )
                          );
      }
      case ZckHelper::PrepareResult::NothingToDo: {
        return setFinished();
      }
      case ZckHelper::PrepareResult::Success:
        // handle down below
        break;
    }

    const auto maxConns = sm._requestDispatcher->maximumConcurrentConnections();
    if ( sm._spec.preferredChunkSize() == 0 ) {
      const auto autoBlkSize = makeBlksize( _fileSize );
      if ( maxConns == -1 ) {
        _preferredChunkSize = autoBlkSize;
      } else {
        _preferredChunkSize = _fileSize / maxConns;
        if ( _preferredChunkSize < autoBlkSize )
          _preferredChunkSize = autoBlkSize;
        else if ( _preferredChunkSize > zypp::ByteCount( 100, zypp::ByteCount::M ) )
          _preferredChunkSize = zypp::ByteCount( 100, zypp::ByteCount::M );
      }
    } else {
      // spec chunk size overrules the automatic one
      _preferredChunkSize = sm._spec.preferredChunkSize();
    }

    std::for_each( prepareRes._blocks.begin (), prepareRes._blocks.end(), [&]( const ZckHelper::Block &block ) {
      _ranges.push_back ( Block{ block } );
    });

    _downloadedMultiByteCount = stateMachine()._spec.headerSize() + prepareRes._bytesReused;
    ensureDownloadsRunning();
  }

  void DLZckState::exit()
  {
    cancelAll( NetworkRequestError() );
  }

  std::shared_ptr<FinishedState> DLZckState::transitionToFinished()
  {
    return std::make_shared<FinishedState>( std::move(_error), stateMachine() );
  }

  void DLZckState::setFinished()
  {
    const auto &spec = stateMachine()._spec;

    std::string err;
    if ( !ZckHelper::validateZckFile ( spec.targetPath(), err ) ) {
      return setFailed ( std::move(err) );
    }

    // everything is valid
    RangeDownloaderBaseState::setFinished();
  }

}

#endif
