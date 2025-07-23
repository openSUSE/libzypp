/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "filestreambuf.h"
#include <zypp-core/zyppng/base/private/linuxhelpers_p.h>

namespace zypp::detail {

  void FdStreamBufImpl::disableAutoClose()
  {
    _fd.resetDispose();
  }

  std::streamsize FdStreamBufImpl::readData( char *buffer_r, std::streamsize maxcount_r )
  {
    if ( !isOpen() || !canRead() )
      return 0;

    const auto r = zyppng::eintrSafeCall( ::read, _fd, buffer_r, maxcount_r );
    if ( r <= 0 )
      return false;

    return true;
  }

  bool FdStreamBufImpl::writeData( const char *buffer_r, std::streamsize count_r )
  {
    if ( !isOpen() || !canWrite() )
      return false;

    std::streamsize written = 0;
    while ( written < count_r ) {
      const auto b =  zyppng::eintrSafeCall( ::write, _fd, buffer_r + written, count_r - written );
      if ( b == -1 ) {
        return false;
      }
      written += b;
    }
    return true;
  }

  bool FdStreamBufImpl::openImpl(int fd, std::ios_base::openmode mode_r)
  {
    _fd = fd;
    _mode = mode_r;
    return true;
  }

  bool FdStreamBufImpl::closeImpl()
  {
    _fd = -1;
    _mode = std::ios_base::openmode(0);
    return true;
  }

}
