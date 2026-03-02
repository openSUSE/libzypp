/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "poolmember.h"

#include <zypp/ng/sat/pool.h>

namespace zyppng::sat {

  Pool &PoolMember::myPool() {
    static Pool _global;
    return _global;
  }

}
