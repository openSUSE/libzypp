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

  namespace {
    Pool &globalPoolInstance() {
      static Pool _global;
      return _global;
    }
  }

  Pool &PoolMember::myPool() {
    return globalPoolInstance ();
  }

  const Pool &PoolMember::myPool() const {
    return globalPoolInstance ();
  }

}
