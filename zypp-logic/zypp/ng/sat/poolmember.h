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

namespace zyppng::sat {

  class Pool;

  class PoolMember
  {
  public:
    static Pool &myPool();
  };

} // namespace zyppng
#endif // POOLMEMBER_H_ZYPPNG_WRAPPER_H
