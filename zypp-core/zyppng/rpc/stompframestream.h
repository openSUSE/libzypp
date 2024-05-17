/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
-----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/

#ifndef ZYPP_CORE_ZYPPNG_RPC_MESSAGESTREAM_H_INCLUDED
#define ZYPP_CORE_ZYPPNG_RPC_MESSAGESTREAM_H_INCLUDED

#include <zypp-core/zyppng/base/Base>
#include <zypp-core/zyppng/base/Signals>
#include <zypp-core/zyppng/base/Timer>
#include <zypp-core/zyppng/io/IODevice>
#include <zypp-core/zyppng/pipelines/expected.h>

#include <zypp-core/rpc/PluginFrame.h>

#include <deque>
#include <optional>

namespace zypp::proto
{
  class Envelope;
}

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS (StompFrameStream);
#if 0
  class InvalidMessageReceivedException : public zypp::Exception
  {
  public:
    InvalidMessageReceivedException( const std::string &msg = {});
  };
#endif

  template <typename T>
  expected<zypp::PluginFrame> toStompMessage( const T& msg ) {
    return msg.toStompMessage();
  }

  template <typename T>
  expected<void> fromStompMessage( const zypp::PluginFrame &message, T &target ) {
    return target.fromStompMessage( message );
  }

  /*!
   *
   * Implements the basic protocol for sending zypp RPC messages over a IODevice
   * using the STOMP frame format as message type.
   */
  class StompFrameStream : public zyppng::Base
  {
    public:

      using Ptr = StompFrameStreamRef;

      /*!
       * Uses the given iostream to send and receive messages.
       * If the device is already open and readable tries to read messages right away.
       * So make sure to check if messages have already been received via \ref nextMessage
       */
      static Ptr create( IODevice::Ptr iostr ) {
        return Ptr( new StompFrameStream( std::move(iostr) ) );
      }

      /*!
       * Returns the next message in the queue, wait for the \ref sigMessageReceived signal
       * to know when new messages have arrived.
       * If \a msgName is specified returns the next message in the queue that matches the msgName
       */
      std::optional<zypp::PluginFrame> nextMessage ( const std::string &msgName = "" );

      /*!
       * Waits until at least one message is in the queue and returns it. Will return a empty
       * optional if a error occurs.
       *
       * If \a msgName is set this will block until a message with the given message name arrives and returns it
       *
       * \note Make sure to check if there are more than one messages in the queue after this function returns
       */
      std::optional<zypp::PluginFrame> nextMessageWait ( const std::string &msgName = "" );

      /*!
       * Send out a RpcMessage to the other side, depending on the underlying device state
       * this will be buffered and send when the device is writeable again.
       */
      bool sendMessage ( const zypp::PluginFrame &message );

      /*!
       * Reads all messages from the underlying IO Device, this is usually called automatically
       * but when shutting down this can be used to process all remaining messages.
       */
      void readAllMessages ();

      /*!
       * Emitted when new messages have arrived. This will continuously be emitted
       * as long as messages are in the queue.
       */
      SignalProxy<void()> sigMessageReceived ();

      /*!
       * Signal is emitted every time there was data on the line that could not be parsed
       */
      SignalProxy<void()> sigInvalidMessageReceived ();

    private:
      StompFrameStream( IODevice::Ptr iostr );
      bool readNextMessage ();
      void timeout( const zyppng::Timer &);

      enum ParserState {
        ReceiveCommand,
        ReceiveHeaders,
        ReceiveBody,
        ParseError
      } _parserState = ReceiveCommand;

      std::optional<zypp::PluginFrame> _pendingMessage;
      std::optional<int64_t> _pendingBodyLen;

      IODevice::Ptr _ioDev;
      Timer::Ptr _nextMessageTimer = Timer::create();
      std::deque<zypp::PluginFrame> _messages;
      Signal<void()> _sigNextMessage;
      Signal<void()> _sigInvalidMessageReceived;

  };
}

#endif // ZYPP_CORE_ZYPPNG_RPC_MESSAGESTREAM_H_INCLUDED
