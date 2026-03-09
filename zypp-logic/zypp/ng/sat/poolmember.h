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
    Pool &myPool();
    const Pool &myPool() const;
  };

} // namespace zyppng
#endif // POOLMEMBER_H_ZYPPNG_WRAPPER_H
