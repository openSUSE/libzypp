/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "namespacecomponent.h"
#include <zypp/ng/sat/namespaces/namespaceprovider.h>
#include <zypp/ng/sat/pool.h>

#include <zypp/ng/idstring.h>
#include <algorithm>

namespace zyppng::sat {

  void NamespaceComponent::registerProvider( detail::IdType namespaceId, std::shared_ptr<NamespaceProvider> provider )
  {
    _providers.insert_or_assign ( namespaceId, provider );
  }

  bool NamespaceComponent::handleNamespace( detail::IdType namespaceId, detail::IdType value  ) const
  {
    auto iter = _providers.find( namespaceId );
    if ( iter != _providers.end() )
      return iter->second->isSatisfied ( value );
    return false;
  }

  void NamespaceComponent::attach(Pool &pool)
  {
    pool.get()->nscallback = &NamespaceComponent::libsolv_callback;
    pool.get()->nscallbackdata = this;
    for ( auto & p : _providers ) {
      p.second->attach(pool);
    }
  }

  void NamespaceComponent::checkDirty( Pool & pool )
  {
    for ( auto & provider : _providers )
      provider.second->checkDirty( pool );
  }

  void NamespaceComponent::prepare( Pool & pool )
  {
    for ( auto & provider : _providers )
      provider.second->prepare( pool );
  }

  void NamespaceComponent::onReset( Pool & pool )
  {
    for ( auto & p : _providers )
      p.second->onReset( pool );
  }

  detail::IdType NamespaceComponent::libsolv_callback(detail::CPool *, void *data, detail::IdType lhs, detail::IdType rhs)
  {
    auto self = static_cast<NamespaceComponent*>(data);
    if ( self && self->handleNamespace( lhs, rhs ) )
      return 1; // RET_systemProperty

    return 0; // RET_unsupported
  }

} // namespace zyppng::sat
