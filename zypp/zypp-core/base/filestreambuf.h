/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_CORE_BASE_FILESTREAMBUF_H
#define ZYPP_CORE_BASE_FILESTREAMBUF_H

#include <zypp-core/AutoDispose.h>
#include <zypp-core/base/SimpleStreambuf>

namespace zypp {
  namespace detail {
    class FdStreamBufImpl {
      public:
        FdStreamBufImpl() {}
        ~FdStreamBufImpl() { closeImpl(); }

        /*!
         * Disables closing of the fd when FdStreamBufImpl
         * is deleted or close() is called
         */
        void disableAutoClose();

        bool
        isOpen   () const
        { return (_fd >= 0); }

        bool
        canRead  () const
        { return( _mode == std::ios_base::in ); }

        bool
        canWrite () const
        { return( _mode == std::ios_base::out ); }

        bool canSeek  ( std::ios_base::seekdir way_r ) const {
          // we could implement seek for regular files, in that case openImpl should
          // check for the file type using fstat and S_ISREG
          return false;
        }

        std::streamsize readData ( char * buffer_r, std::streamsize maxcount_r  );

        bool writeData( const char * buffer_r, std::streamsize count_r );

        off_t seekTo( off_t off_r, std::ios_base::seekdir way_r, std::ios_base::openmode omode_r ) {
          return -1;
        }

        off_t tell() const {
          return -1;
        }

      protected:
        bool openImpl( int fd, std::ios_base::openmode mode_r );
        bool closeImpl ();  // closes the file

    private:
        zypp::AutoFD _fd = -1;
        std::ios_base::openmode  _mode = std::ios_base::openmode(0);
    };
  }

  using FdStreamBuf = detail::SimpleStreamBuf<detail::FdStreamBufImpl>;
}
#endif // ZYPP_CORE_BASE_FILESTREAMBUF_H
