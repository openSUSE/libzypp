/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_MEDIA_FILECHECKEXCEPTION_H
#define ZYPP_MEDIA_FILECHECKEXCEPTION_H

#include <zypp-core/base/Exception.h>

namespace zypp {

  class FileCheckException : public Exception
  {
  public:
    FileCheckException(std::string msg)
      : Exception(std::move(msg))
    {}
  };

  class CheckSumCheckException : public FileCheckException
  {
  public:
    CheckSumCheckException(std::string msg)
      : FileCheckException(std::move(msg))
    {}
  };

  class SignatureCheckException : public FileCheckException
  {
  public:
    SignatureCheckException(std::string msg)
      : FileCheckException(std::move(msg))
    {}
  };

}

#endif // ZYPP_MEDIA_FILECHECKEXCEPTION_H
