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
module;
#include <string_view>
#include <initializer_list>

export module zyppng:sat_namespaces_namespaceprovider;

import :sat_poolconstants;

export namespace zyppng::sat {

  class Pool;

  /**
   * \brief Abstract base class for logic providing data to a libsolv namespace.
   *
   * A NamespaceProvider acts as the "Source of Truth" for specific dependency
   * types (e.g., Locales, Modaliases, or Filesystem paths). It decouples
   * high-level configuration logic from the low-level pool implementation.
   *
   * \note This class maintains a 1:1 relationship with a NamespaceComponent
   * via a dirty-notification callback.
   */
  class NamespaceProvider {
    public:
      virtual ~NamespaceProvider() = default;

      /**
       * \brief Check if a specific value satisfies this namespace condition.
       * \param value The id value to check (e.g., "en_US" for a locale namespace).
       * \return True if the condition is met, false otherwise.
       * * \details This method is typically called via the libsolv namespace callback
       * during dependency resolution.
       */
      virtual bool isSatisfied( detail::IdType value ) const;

      /**
       * @brief checkDirty
       *
       * Called before the pool prepare sequence starts. This is the correct
       * place to probe external state and call pool.setDirty() if needed.
       * The default implementation does nothing.
       */
      virtual void checkDirty( Pool & /*pool*/ ) {}

      /**
       * @brief prepare
       *
       * Called during the pre-index phase of pool preparation, after
       * checkDirty() and before pool_createwhatprovides(). Use this to
       * set up any state the libsolv namespace callback needs to answer
       * queries correctly during index construction.
       * The default implementation does nothing.
       */
      virtual void prepare( Pool & /*pool*/ ) {}

      virtual void onReset( Pool & /*pool*/ ) {}

      /*!
       * Attaching to a pool, default just stores a backlink pointer to the pool.
       */
      virtual void attach(Pool &pool);

    protected:
      /**
       * \brief Notify the attached registry/pool that data has changed.
       * * Derived classes must call this whenever their internal configuration
       * (e.g., the set of active locales) is updated to ensure the libsolv
       * caches are invalidated.
       */
      void notifyDirty( PoolInvalidation invalidationLevel , std::initializer_list<std::string_view> reasons );

    protected:
      Pool *_pool = nullptr;

  };

} // namespace zyppng::sat

