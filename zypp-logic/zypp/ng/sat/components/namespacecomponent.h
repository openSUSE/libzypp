/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPP_NG_SAT_COMPONENTS_NAMESPACECOMPONENT_H_INCLUDED
#define ZYPP_NG_SAT_COMPONENTS_NAMESPACECOMPONENT_H_INCLUDED

#include <memory>
#include <unordered_map>
#include <zypp/ng/sat/poolconstants.h>
#include "poolcomponents.h"

namespace zyppng::sat {

  class NamespaceProvider;

  /**
   * \brief Mediator managing the collection of NamespaceProviders for a Pool.
   *
   * The NamespaceComponent acts as the central hub for libsolv namespace callbacks.
   * It maps libsolv Namespace IDs to specific \ref NamespaceProvider instances
   * and propagates dirty notifications up to the Pool.
   *
   * By keeping this logic separate from the Pool, we prevent high-level
   * dependencies (like Locales or Hardware detection) from leaking into the
   * low-level pool header.
   */
  class NamespaceComponent : public IPoolComponent {
    public:

      ~NamespaceComponent() override = default;

      /**
       * \brief Register a provider for a specific namespace.
       * \param namespaceId The ID representing the namespace (e.g., 'namespace:language').
       * \param provider A shared pointer to the provider implementation.
       */
      void registerProvider( detail::IdType namespaceId, std::shared_ptr<NamespaceProvider> provider );

      /**
       * \brief The entry point for the libsolv namespace callback.
       * \param namespaceId The ID of the namespace being queried.
       * \param value The value to be checked.
       * \return True if any registered provider satisfies the condition.
       */
      bool handleNamespace( detail::IdType namespaceId, detail::IdType value ) const;

      // --- IPoolComponent implementation ---

      ComponentStage stage() const override { return ComponentStage::Environment; }
      int priority() const override { return -10; } // Run early in Environment stage

      void attach( Pool & pool ) override;
      void prepare( Pool & pool ) override;

    private:

      /** The static C-style callback for libsolv */
      static detail::IdType libsolv_callback( detail::CPool *, void * data, detail::IdType lhs, detail::IdType rhs );

      /** \brief Internal map of IDs to their respective providers. */
      std::unordered_map<detail::IdType, std::shared_ptr<NamespaceProvider>> _providers;
  };

} // namespace zyppng::sat

#endif