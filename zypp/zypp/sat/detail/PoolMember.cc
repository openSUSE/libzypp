/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "PoolMember.h"
#include "PoolImpl.h"

namespace zypp::sat::detail {

  static bool & poolDestroyed() {
      static bool _flag = false;   // trivially destructible — never "destroyed"
      return _flag;
  }

  static PoolImpl & globalPool()
  {
    struct Guard {
        Guard() = default;
        Guard(const Guard &) = delete;
        Guard(Guard &&) = delete;
        Guard &operator=(const Guard &) = delete;
        Guard &operator=(Guard &&) = delete;
        ~Guard() { poolDestroyed() = true; }
    };

    static PoolImpl _global;
    static Guard _guard;
    return _global;
  }


  ///////////////////////////////////////////////////////////////////
  //
  //	METHOD NAME : PoolMember::myPool
  //	METHOD TYPE : PoolImpl
  //
  PoolImpl & PoolMember::myPool()
  {
    if ( poolDestroyed () )
      ZYPP_THROW( zypp::Exception("Global PoolImpl instance used after it was destroyed!") );
    return globalPool ();
  }

  bool PoolMember::poolValid()
  {
    return !poolDestroyed ();
  }
}
