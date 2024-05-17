/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/

#include "stompframestream.h"
#include <zypp-core/ByteCount.h>
#include <zypp-core/zyppng/core/string.h>

#include <zypp-core/AutoDispose.h>
#include <zypp-core/zyppng/base/AutoDisconnect>

namespace zyppng {

  constexpr auto MAX_CMDLEN  = 256;
  constexpr auto MAX_HDRLEN  = 8 * 1024;    // we might send long paths in headers
  constexpr auto MAX_BODYLEN = 1024 * 1024; // 1Mb for now, we do not want to use up all the memory

#if 0
  InvalidMessageReceivedException::InvalidMessageReceivedException( const std::string &msg )
    : zypp::Exception( zypp::str::Str() << "Invalid Message received: (" << msg <<")" )
  { }
#endif


  zyppng::StompFrameStream::StompFrameStream( IODevice::Ptr iostr ) : _ioDev( std::move(iostr) )
  {
    connect( *_nextMessageTimer, &Timer::sigExpired, *this, &StompFrameStream::timeout );
    _nextMessageTimer->setSingleShot(false);

    connect( *_ioDev, &IODevice::sigReadyRead, *this, &StompFrameStream::readAllMessages );
    if ( _ioDev->isOpen () && _ioDev->canRead () )
      readAllMessages ();
  }

  bool StompFrameStream::readNextMessage( )
  {
    const auto &parseError = [this](){
      _parserState = ParseError;
      _pendingMessage.reset();
      _pendingBodyLen.reset();
      _sigInvalidMessageReceived.emit();
    };

    // ATTENTION: Remember to also update the parser logic in zypp-core/rpc/PluginFrame.cc
    //            if code here is changed or features are added.

    // loop until we have a full message, or we have no more data to read
    while(true) {
      switch( _parserState ) {
        case ParseError: {
          // we got a parse error before, try to recover by reading until the next \0
          bool gotTerm = false;
          while ( _ioDev->bytesAvailable ( ) ) {
            auto d = _ioDev->read (1);
            if ( !d.size() )
              break;

            if ( d.front () == '\0' ){
              gotTerm = true;
              _parserState = ReceiveCommand;
              break;
            }
          }

          if ( gotTerm )
            continue;

          return false;
        }
        case ReceiveCommand: {
          const auto ba = _ioDev->readBufferCount();
          if ( !_ioDev->canReadLine() ) {
            if ( ba > MAX_CMDLEN ) {
              ERR << "Received malformed message from peer, CMD line exceeds: " << MAX_CMDLEN << " bytes" << std::endl;
              parseError();
              continue;
            }
            return false;
          }

          ByteArray command = _ioDev->readLine( MAX_CMDLEN );
          command.pop_back(); // remove \n
          if ( command.empty() ) {
            // STOMP spec says multiple EOLs after a message are allowed, so we just ignore empty lines
            // if they happen before a new frame starts
            WAR << "Received empty line before command, ignoring" << std::endl;
            return false;
          }

          if ( !_pendingMessage )
            _pendingMessage = zypp::PluginFrame();
          _pendingMessage->setCommand( command.asString() );
          _parserState = ReceiveHeaders;

          break;
        }

        case ReceiveHeaders: {
          const auto ba = _ioDev->readBufferCount();
          if ( !_ioDev->canReadLine() ) {
            if ( ba > MAX_HDRLEN ) {
              ERR << "Received malformed message from peer, header line exceeds: " << MAX_HDRLEN << " bytes" << std::endl;
              parseError();
              continue;
            }
            return false;
          }

          ByteArray header = _ioDev->readLine( MAX_HDRLEN );
          header.pop_back(); // remove \n
          if ( header.empty () ) {
            // --> empty line sep. header and body
            _parserState = ReceiveBody;

            // if we received a content-length header we set the flag for the body parser to know it has to read
            // n bytes before expecting the \0 terminator
            const auto &contentLen = _pendingMessage->getHeaderNT( zypp::PluginFrame::contentLengthHeader(), std::string() );
            std::optional<uint64_t> cLen;
            if ( !contentLen.empty() ) {
              cLen = zyppng::str::safe_strtonum<uint64_t>(contentLen);
              if ( !cLen ) {
                ERR << "Received malformed message from peer: Invalid value for " << zypp::PluginFrame::contentLengthHeader() << ":" << contentLen << std::endl;
                parseError();
                continue;
              }

              if ( (*cLen) > MAX_BODYLEN ) {
                ERR << "Message body exceeds maximum length: " << zypp::ByteCount( *cLen ) << " vs " << zypp::ByteCount( MAX_BODYLEN ) << std::endl;
                parseError();
                continue;
              }

              _pendingBodyLen = *cLen;
              _pendingMessage->clearHeader( zypp::PluginFrame::contentLengthHeader() );
            }
          } else {
            try {
              _pendingMessage->addRawHeader ( header );
            } catch ( const zypp::Exception &e ) {
              ZYPP_CAUGHT(e);
              ERR << "Received malformed message from peer, header format invalid: " << header.asStringView() << " (" << e << ")" << std::endl;
              parseError();
              continue;
            }
          }
          break;
        }

        case ReceiveBody: {

          ByteArray body;
          if ( _pendingBodyLen ) {
            // we need to read the required body bytes plus the terminating \0
            const auto reqBytes = (*_pendingBodyLen) + 1;
            if ( _ioDev->bytesAvailable() < reqBytes )
              return false;

            body = _ioDev->read( reqBytes );
            if ( body.back () != '\0' ) {
              ERR << "Received malformed message from peer: Body was not terminated with \\0" << std::endl;
              parseError();
              continue;
            }

            body.pop_back (); // remove \0

          } else {
            // we do not know the body size, need to read until \0
            const auto ba = _ioDev->readBufferCount();
            if ( !_ioDev->canReadUntil( _ioDev->currentReadChannel (), '\0' ) ) {
              if ( ba > MAX_BODYLEN ) {
                ERR << "Message body exceeds maximum length: " << zypp::ByteCount( _ioDev->readBufferCount() ) << " vs " << zypp::ByteCount( MAX_BODYLEN ) << std::endl;
                parseError();
                continue;
              }
              return false;
            }

            body = _ioDev->channelReadUntil( _ioDev->currentReadChannel (), '\0' );
            body.pop_back(); // remove the \0
          }

          // if we reach this place we have a full message, store the body and lets go)
          _pendingMessage->setBody( std::move(body) );

          _messages.emplace_back( std::move(*_pendingMessage) );
          _pendingMessage.reset();
          _pendingBodyLen.reset();
          _parserState = ReceiveCommand;

          _sigNextMessage.emit ();

          if ( _messages.size() ) {
            // nag the user code until all messages have been used up
            _nextMessageTimer->start(0);
          }

          // once we have a message, exit the loop so other things can be done
          return true;
        }
      }
    }
  }

  void StompFrameStream::timeout(const Timer &)
  {
    if ( _messages.size() )
      _sigNextMessage.emit();

    if ( !_messages.size() )
      _nextMessageTimer->stop();
  }

  std::optional<zypp::PluginFrame> zyppng::StompFrameStream::nextMessage( const std::string &msgName )
  {
    if ( !_messages.size () ) {

      // try to read the next messages from the fd
      {
        _sigNextMessage.block ();
        zypp::OnScopeExit unblock([&](){
          _sigNextMessage.unblock();
        });
        readAllMessages();
      }

      if ( !_messages.size () )
        return {};
    }

    std::optional<zypp::PluginFrame>  res;

    if( msgName.empty() ) {
      res = std::move( _messages.front () );
      _messages.pop_front();

    } else {
      const auto i = std::find_if( _messages.begin(), _messages.end(), [&]( const zypp::PluginFrame &msg ) {
        return msg.command() == msgName;
      });

      if ( i != _messages.end() ) {
        res = std::move(*i);
        _messages.erase(i);
      }
    }

    if ( _messages.size() )
      _nextMessageTimer->start(0);
    else
      _nextMessageTimer->stop();

    return res;
  }

  std::optional<zypp::PluginFrame> StompFrameStream::nextMessageWait( const std::string &msgName )
  {
    // make sure the signal is not emitted until we have the next message
    _sigNextMessage.block ();
    zypp::OnScopeExit unblock([&](){
      _sigNextMessage.unblock();
    });

    bool receivedInvalidMsg = false;
    AutoDisconnect defered( connectFunc( *this, &StompFrameStream::sigInvalidMessageReceived, [&](){
      receivedInvalidMsg = true;
    }));

    const bool hasMsgName = msgName.size();
    while ( !receivedInvalidMsg && _ioDev->isOpen() && _ioDev->canRead() ) {
      if ( _messages.size() ) {
        if ( hasMsgName ) {
          std::optional<zypp::PluginFrame> msg = nextMessage(msgName);
          if ( msg ) return msg;
        }
        else {
          break;
        }
      }

      if ( !_ioDev->waitForReadyRead ( -1 ) ) {
        // this can only mean that a error happened, like device was closed
        return {};
      }
    }
    return nextMessage (msgName);
  }

  bool zyppng::StompFrameStream::sendMessage( const zypp::PluginFrame &env )
  {
    if ( !_ioDev->canWrite () )
      return false;

    try {
      IODeviceOStreamBuf ostrbuf(_ioDev);
      std::ostream output(&ostrbuf);
      env.writeTo ( output );
    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT(e);
      ERR << "Failed to serialize message to stream" << std::endl;
      return false;
    }

    return true;
  }

  SignalProxy<void ()> zyppng::StompFrameStream::sigMessageReceived()
  {
    return _sigNextMessage;
  }

  SignalProxy<void ()> StompFrameStream::sigInvalidMessageReceived()
  {
    return _sigInvalidMessageReceived;
  }

  void StompFrameStream::readAllMessages()
  {
    bool cont = true;
    while ( cont && _ioDev->bytesAvailable() ) {
      cont = readNextMessage ();
    }
  }
}
