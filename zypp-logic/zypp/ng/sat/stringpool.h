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
#ifndef ZYPPNG_SAT_STRINGPOOL_H_INCLUDED
#define ZYPPNG_SAT_STRINGPOOL_H_INCLUDED

#include <zypp/ng/sat/poolconstants.h>

namespace zyppng::sat {

  /**
   * \brief Singleton manager for the underlying libsolv string pool.
   *
   * This class provides a centralized, low-level wrapper around the \ref CPool
   * structure. It acts as the "Source of Truth" for string resolution in the
   * zypp architecture, ensuring that ID-to-string mappings remain consistent.
   *
   * \note This class is strictly non-copyable and non-movable to ensure
   * the integrity of the underlying libsolv state.
   *
   * \note currently this class creates the one single pool libzypp currently
   * supports until libsolv supports sharing the same stringpool for different pool instances.
   * Do not depend on this pool being also the pool for solving!
   */
  class StringPool {

    public:
      /**
       * \brief Access the global StringPool instance.
       */
      static StringPool &instance();

      /**
       * \brief Destructor.
       * \details Handles the safe cleanup of the underlying \ref CPool resources.
       */
      ~StringPool();

      /**
       * \brief Pointer style access forwarded to the raw sat-pool.
       * \return A pointer to the underlying \ref CPool.
       */
      detail::CPool * operator->()
      { return _pool; }

      /**
       * \brief Explicit accessor for the raw sat-pool.
       * \return A pointer to the underlying \ref CPool.
       */
      detail::CPool * getPool() const
      { return _pool; }

      /** libsolv capability parser */
      detail::IdType parserpmrichdep( const char * capstr_r )
      { return ::pool_parserpmrichdep( _pool, capstr_r ); }

      /* Delete copy and move semantics to maintain singleton integrity */
      StringPool(const StringPool &) = delete;
      StringPool(StringPool &&) = delete;
      StringPool &operator=(const StringPool &) = delete;
      StringPool &operator=(StringPool &&) = delete;

    private:
      StringPool();

    private:
      /** * \brief Internal pointer to the libsolv pool structure (\ref CPool).
       */
      detail::CPool * _pool;
  };

}
#endif

