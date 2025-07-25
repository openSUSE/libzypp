#ifndef ASYNCDATASOURCE_H
#define ASYNCDATASOURCE_H

#include <zypp-core/zyppng/base/Base>
#include <zypp-core/zyppng/io/IODevice>

namespace zyppng {

  class AsyncDataSourcePrivate;

  class AsyncDataSource : public IODevice
  {
    ZYPP_DECLARE_PRIVATE(AsyncDataSource)
  public:

    enum ChannelCloseReason {
      RemoteClose,    // the other side of the fd was closed
      AccessError,    // we got an EACCESS when polling the fd
      InternalError,  // we got a unexpected errno when polling the fd
      UserRequest     // channel were closed because close() was called
    };

    using Ptr     = std::shared_ptr<AsyncDataSource>;
    using WeakPtr = std::weak_ptr<AsyncDataSource>;

    static Ptr create ();

    /*!
     * Borrows the given fd's and starts reading/writing to them if data is available.
     * Note that \ref AsyncDataSource does not take ownership of the FDs, they need to be closed
     * explicitely after they were released with \ref close
     */
    bool openFds ( const std::vector<int>& readFds, int writeFd = -1 );

    /*!
     * Closes the \ref AsyncDataSource and releases the used fd's to the calling code. They are NOT
     * closed.
     */
    void close () override;

    using IODevice::waitForReadyRead;

    virtual void closeWriteChannel ();

    /*!
     * Blocks the current event loop to wait until all bytes currently in the buffer have been written to
     * the write fd.
     *
     * \note do not use until there is a very good reason, like event processing should not continue until readyRead was sent
     */
    bool waitForReadyRead(uint channel, int timeout) override;

    /*!
     * Blocks the current event loop to wait until all bytes currently in the buffer have been written to
     * the write fd.
     *
     * \note do not use until there is a very good reason, like event processing should not continue until bytesWritten was sent
     */
    void flush ();

    /*!
     * Signal is emitted always when the write channel is closed.
     * All data that was not written yet will be discarded and canWrite() will return
     * false.
     */
    SignalProxy<void( AsyncDataSource::ChannelCloseReason )> sigWriteFdClosed();

    /*!
     * Signal is emitted always when the write channel is closed, for example
     * when the write side of a pipe is closed. All data still residing in the read buffer
     * can still be read.
     */
    SignalProxy<void( uint, AsyncDataSource::ChannelCloseReason )> sigReadFdClosed();

    /*!
     * Returns true as long as the default read channel was not closed ( e.g. sigReadFdClosed was emitted )
     */
    bool readFdOpen () const;

    /*!
     * Returns true as long as the given read channel was not closed ( e.g. sigReadFdClosed was emitted )
     */
    bool readFdOpen ( uint channel ) const;

    /*!
     * Returns the current number of bytes that are not yet written to the data source.
     */
    int64_t bytesPending() const override;

  protected:
    AsyncDataSource (  );
    AsyncDataSource( AsyncDataSourcePrivate &d );
    int64_t writeData(const char *data, int64_t count) override;

  private:
    using IODevice::open;

    int64_t readData( uint channel, char *buffer, int64_t bufsize ) override;
    int64_t rawBytesAvailable( uint channel ) const override;
    void readChannelChanged ( uint channel ) override;
  };
}


#endif // ASYNCDATASOURCE_H
