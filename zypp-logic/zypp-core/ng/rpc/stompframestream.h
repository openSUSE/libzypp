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

#ifndef ZYPP_CORE_ZYPPNG_RPC_STOMPFRAMESTREAM_H_INCLUDED
#define ZYPP_CORE_ZYPPNG_RPC_STOMPFRAMESTREAM_H_INCLUDED

#include <zypp-core/TriBool.h>
#include <zypp-core/ng/base/Base>
#include <zypp-core/ng/core/String>
#include <zypp-core/ng/base/Signals>
#include <zypp-core/ng/base/Timer>
#include <zypp-core/ng/io/IODevice>
#include <zypp-core/ng/pipelines/expected.h>

#include <zypp-core/rpc/PluginFrame.h>

#include <deque>
#include <optional>

namespace zyppng {

  ZYPP_FWD_DECL_TYPE_WITH_REFS (StompFrameStream);

  class ZYPP_API InvalidMessageReceivedException : public zypp::Exception
  {
  public:
    InvalidMessageReceivedException( const std::string &msg = {});
  };

  namespace rpc {

    template <typename T>
    expected<zypp::PluginFrame> toStompMessage( const T& msg ) {
      return msg.toStompMessage();
    }

    template <typename T>
    expected<T> fromStompMessage( const zypp::PluginFrame &message ) {
      return T::fromStompMessage( message );
    }

    // Reads data from the stomp message and converts it to the target type
    // used to read header values and values serialized into a terminated data field
    template <typename T>
    void parseDataIntoField( const std::string &headerVal, T &target  )
    {
      if constexpr ( std::is_same_v<bool, T> ) {
        const auto &triBool = zypp::str::strToTriBool ( headerVal );
        if ( indeterminate(triBool) ) {
          ZYPP_THROW ( zypp::PluginFrameException( "Invalid value for boolean field" ) );
        }
        target = bool(triBool);
      } else if constexpr ( std::is_same_v<std::string, T> ) {
        target = headerVal;
      } else {
        // numbers
        auto val = zyppng::str::safe_strtonum<T> ( headerVal );
        if ( !val )
          ZYPP_THROW ( zypp::PluginFrameException( "Invalid value for numerical field" ) );
        target = *val;
      }
    }

    // queries a header value and calls parseDataIntoField on it
    template <typename T>
    void parseHeaderIntoField( const zypp::PluginFrame &msg, const std::string &name, T &target  )
    {
      return parseDataIntoField ( msg.getHeader(name), target );
    }

    /**
     * @brief Constructs a zypp::PluginFrame based on the static type information of T.
     *
     * This helper function creates a new PluginFrame, initializing its command/header
     * using the string data provided by the template argument's `typeName` member.
     *
     * @tparam T A type representing a command or protocol message.
     * T must satisfy the following requirements:
     * - Must have a static public member named `typeName`.
     * - `typeName` must provide `.data()` (returning char*) and `.length()` (returning size_t).
     * - Typically, `typeName` is a `std::string_view` or `constexpr` string wrapper.
     *
     * @return zypp::PluginFrame A generic frame initialized with T's command string.
     */
    template <typename T>
    inline zypp::PluginFrame prepareFrame() {
      return zypp::PluginFrame ( std::string( T::typeName.data(), T::typeName.length() ) );
    }

  }

  /*!
   *
   * Implements the basic protocol for sending zypp RPC messages over a IODevice
   * using the STOMP frame format as message type.
   */
  class ZYPP_API StompFrameStream : public zyppng::Base
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
       * Send out a PluginFrame to the other side, depending on the underlying device state
       * this will be buffered and send when the device is writeable again.
       */
      bool sendFrame ( const zypp::PluginFrame &message );

      template <typename T>
      bool sendMessage ( const T &message )
      {
        if constexpr ( std::is_same_v<T, zypp::PluginFrame> ) {
          return sendFrame( message );
        } else {
          const auto &msg = rpc::toStompMessage(message);
          if ( !msg ) {
            ERR << "Failed to serialize message" << std::endl;
            return false;
          }
          return sendFrame( *msg );
        }
      }

      template<class T>
      static expected< T > parseMessage ( const zypp::PluginFrame &m ) {
        return rpc::fromStompMessage<T>(m);
      }

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

#endif // ZYPP_CORE_ZYPPNG_RPC_STOMPFRAMESTREAM_H_INCLUDED
