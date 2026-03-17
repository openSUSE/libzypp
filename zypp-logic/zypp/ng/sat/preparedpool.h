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
#ifndef ZYPP_NG_SAT_PREPAREDPOOL_H_INCLUDED
#define ZYPP_NG_SAT_PREPAREDPOOL_H_INCLUDED

#include <zypp/ng/sat/poolconstants.h>
#include <zypp/ng/sat/queue.h>
#include <zypp/ng/sat/solvattr.h>
#include <zypp/ng/base/serialnumber.h>

namespace zyppng::sat {

  class Pool;
  class Capability;
  class Solvable;

  /**
   * \class PreparedPool
   * \brief A move-only, non-owning view of a Pool that guarantees the
   *        whatprovides index is valid.
   *
   * \par Obtaining a PreparedPool
   * A \c PreparedPool is obtainable \b only as the return value of
   * \ref Pool::prepare().  It cannot be default-constructed or copied.
   *
   * \par Ownership
   * \c PreparedPool is non-owning.  Destroying it does \b nothing to the
   * underlying libsolv index — the index lives inside the libsolv \c ::Pool
   * struct and is managed by the owning \ref Pool.
   *
   * \par Thread safety
   * \c PreparedPool is a non-owning view.  Concurrent reads are safe as long
   * as the owning \ref Pool is not mutated concurrently.
   *
   * \par Debug invariant
   * In debug builds the serial number of the pool is captured at construction.
   * Every query asserts the serial has not changed, catching use-after-invalidate.
   */
  class PreparedPool
  {
  public:
    PreparedPool() = delete;
    PreparedPool( const PreparedPool & ) = delete;
    PreparedPool & operator=( const PreparedPool & ) = delete;

    PreparedPool( PreparedPool && ) = default;
    PreparedPool & operator=( PreparedPool && ) = default;

    ~PreparedPool() = default;

    /** Access the owning Pool. */
    Pool & pool() const noexcept { return _pool; }

    /** Expert backdoor — raw libsolv pool pointer. */
    detail::CPool * get() const noexcept;

    /** \name whatprovides queries
     *
     * These methods require the whatprovides index to be valid, which is
     * guaranteed by the type invariant of \c PreparedPool.
     */
    //@{

    /** Returns the offset into the internal whatprovidesdata array for \a cap_r.
     *  Use \ref whatProvidesData to iterate the stored ids. */
    unsigned whatProvidesCapabilityId( detail::IdType cap_r ) const;

    /** Returns the id stored at \a offset_r in the whatprovidesdata array.
     *  Iterate from \ref whatProvidesCapabilityId until a \c 0 id is returned. */
    detail::IdType whatProvidesData( unsigned offset_r ) const;

    /** All solvables whose attribute \a attr_r matches dependency \a cap_r. */
    Queue whatMatchesDep    ( const SolvAttr &, const Capability & ) const;

    /** All solvables whose attribute \a attr_r matches solvable \a solv_r. */
    Queue whatMatchesSolvable( const SolvAttr &, const Solvable & )  const;

    /** All solvables whose attribute \a attr_r contains dependency \a cap_r. */
    Queue whatContainsDep   ( const SolvAttr &, const Capability & ) const;
    //@}

  private:
    friend class Pool;

    /** Only Pool::prepare() may construct a PreparedPool. */
    explicit PreparedPool( Pool & pool_r ) noexcept;

    Pool & _pool;
#ifndef NDEBUG
    SerialNumberWatcher _serialWatcher;
#endif
  };

} // namespace zyppng::sat

#endif // ZYPP_NG_SAT_PREPAREDPOOL_H_INCLUDED
