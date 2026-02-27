/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef POOLMEMBER_H_ZYPPNG_WRAPPER_H
#define POOLMEMBER_H_ZYPPNG_WRAPPER_H

#include <zypp/ng/sat/poolbase.h>

namespace zyppng::sat
{
  class PoolMember {
    public:
      static PoolBase &myPool() {
        static PoolBase _global;
        return _global;
      }
  };
} // namespace zyppng
#endif // POOLMEMBER_H_ZYPPNG_WRAPPER_H
