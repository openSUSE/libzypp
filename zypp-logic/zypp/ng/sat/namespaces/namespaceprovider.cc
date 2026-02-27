/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "namespaceprovider.h"

#include <zypp/ng/sat/poolbase.h>
#include <zypp-core/base/LogTools.h>

namespace zyppng::sat {

  bool NamespaceProvider::isSatisfied( detail::IdType ) const
  {
    return false;
  }

  void NamespaceProvider::attach(PoolBase &pool)
  {
    _pool = &pool;
  }

  void NamespaceProvider::notifyDirty( PoolInvalidation invalidationLevel, std::initializer_list<std::string_view> reasons )
  {
    if (!_pool) {
      ERR << "NamespaceProvider not attached, can not send the invalidation request" << std::endl;
      return;
    }
    _pool->setDirty ( invalidationLevel, std::move(reasons) );
  }

}
