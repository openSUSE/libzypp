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
#ifndef ZYPP_NG_SAT_NAMESPACES_NAMESPACEPROVIDER_H_INCLUDED
#define ZYPP_NG_SAT_NAMESPACES_NAMESPACEPROVIDER_H_INCLUDED

#include <string_view>
#include <initializer_list>
#include <zypp/ng/sat/poolconstants.h>

namespace zyppng::sat {

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
       * @brief prepare
       *
       * Called to notify providers when the pool is being prepared,
       * this is the right place to check if the pool or the dependencies should be marked dirty,
       * the default implementation just returns true.
       *
       * In case of a pool invalidation, return \a false so that the prepare loop can start over.
       */
      virtual bool prepare( Pool & /*pool*/ ) { return true; }

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

#endif
