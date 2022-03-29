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
#include <zypp-proto/envelope.pb.h>
#include <zypp-core/zyppng/rpc/rpc.h>

#include <deque>
#include <optional>

namespace zyppng {

  using RpcMessage = zypp::proto::Envelope;

  namespace rpc {
    /*!
     * Helper function to get the type name of a given RPC message type.
     * Sadly Protobuf does not offer a static function to get the types FQN we
     * cache it after asking for it the first time. So we need a dummy object just once.
     */
    template <typename T>
    const std::string & messageTypeName() {
      static std::string name = T().GetTypeName();
      return name;
    }
  }

  class InvalidMessageReceivedException : public zypp::Exception
  {
  public:
    InvalidMessageReceivedException( const std::string &msg = {});
  };


  /*!
   *
   * Implements the basic protocol for sending zypp RPC messages over a IODevice
   *
   * Communication Format:
   * ---------------------
   * Each message is serialized into a zypp.proto.Envelope and sent over the communication medium in binary
   * format. The binary format looks like:
   *
   * +--------------------------------+---------------------------------+
   * | msglen ( 32 bit unsigned int ) | binary zypp.proto.Envelope data |
   * +--------------------------------+---------------------------------+
   *
   * The header defines the size in bytes of the following data trailer. The header type is a 32 bit uint, endianess is defined by
   * the underlying CPU arch. The data portion is directly generated by libprotobuf via SerializeToZeroCopyStream() to generate
   * the binary represenation of the message.
   *
   */
  class RpcMessageStream : public zyppng::Base
  {
    public:

      using Ptr = std::shared_ptr<RpcMessageStream>;

      /*!
       * Uses the given iostream to send and receive messages.
       * If the device is already open and readable tries to read messages right away.
       * So make sure to check if messages have already been received via \ref nextMessage
       */
      static Ptr create( IODevice::Ptr iostr ) {
        return Ptr( new RpcMessageStream( std::move(iostr) ) );
      }

      /*!
       * Returns the next message in the queue, wait for the \ref sigMessageReceived signal
       * to know when new messages have arrived.
       * If \a msgName is specified returns the next message in the queue that matches the msgName
       */
      std::optional<RpcMessage> nextMessage ( const std::string &msgName = "" );

      /*!
       * Waits until at least one message is in the queue and returns it. Will return a empty
       * optional if a error occurs.
       *
       * If \a msgName is set this will block until a message with the given message name arrives and returns it
       *
       * \note Make sure to check if there are more than one messages in the queue after this function returns
       */
      std::optional<RpcMessage> nextMessageWait ( const std::string &msgName = "" );

      /*!
       * Send out a RpcMessage to the other side, depending on the underlying device state
       * this will be buffered and send when the device is writeable again.
       */
      bool sendMessage ( const RpcMessage &env );

      /*!
       * Reads all messages from the underlying IO Device, this is usually called automatically
       * but when shutting down this can be used to process all remaining messages.
       */
      void readAllMessages ();

      /*!
       * Send a messagee to the server side, it will be enclosed in a Envelope
       * and immediately sent out.
       */
      template <typename T>
      std::enable_if_t< !std::is_same_v<T, RpcMessage>, bool> sendMessage ( const T &m ) {
        RpcMessage env;

        env.set_messagetypename( m.GetTypeName() );
        m.SerializeToString( env.mutable_value() );

        return sendMessage ( env );
      }

      /*!
       * Emitted when new messages have arrived. This will continuously be emitted
       * as long as messages are in the queue.
       */
      SignalProxy<void()> sigMessageReceived ();

      /*!
       * Signal is emitted every time there was data on the line that could not be parsed
       */
      SignalProxy<void()> sigInvalidMessageReceived ();

      template<class T>
      static expected< T > parseMessage ( const RpcMessage &m ) {
        T p;
        if ( !p.ParseFromString( m.value() ) ) {
          const auto &msg = zypp::str::Str() << "Failed to parse " << m.messagetypename() << " message.";
          ERR << msg << std::endl ;
          return expected<T>::error( ZYPP_EXCPT_PTR ( InvalidMessageReceivedException(msg) ) );
        }
        return expected<T>::success( std::move(p) );
      }

      template<class T>
      static expected< void > parseMessageInto ( const RpcMessage &m, T &target ) {
        if ( !target.ParseFromString( m.value() ) ) {
          const auto &msg = zypp::str::Str() << "Failed to parse " << m.messagetypename() << " message.";
          ERR << msg << std::endl ;
          return expected<void>::error( ZYPP_EXCPT_PTR ( InvalidMessageReceivedException(msg) ) );
        }
        return expected<void>::success( );
      }

    private:
      RpcMessageStream( IODevice::Ptr iostr );
      bool readNextMessage ();
      void timeout( const zyppng::Timer &);

      IODevice::Ptr _ioDev;
      Timer::Ptr _nextMessageTimer = Timer::create();
      zyppng::rpc::HeaderSizeType _pendingMessageSize = 0;
      std::deque<RpcMessage> _messages;
      Signal<void()> _sigNextMessage;
      Signal<void()> _sigInvalidMessageReceived;

  };
}



#endif // ZYPP_CORE_ZYPPNG_RPC_MESSAGESTREAM_H_INCLUDED
