/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/ng/sat/solvablespec.h
 *
 * This file contains private API, this might break at any time between releases.
 * You have been warned!
 */
#ifndef ZYPP_NG_SAT_SOLVABLESPEC_H_INCLUDED
#define ZYPP_NG_SAT_SOLVABLESPEC_H_INCLUDED

#include <zypp/ng/idstring.h>
#include <zypp/ng/sat/capability.h>
#include <zypp/ng/sat/poolconstants.h>
#include <zypp-core/base/inputstream.h>

#include <unordered_set>

namespace zyppng::sat {

  class Pool;
  class Solvable;

  /**
   * \class SolvableSpec
   * \brief A pure data container describing a set of solvables by ident and/or provides.
   *
   * \par Design
   * This is the NG reimplementation of the legacy \c zypp::sat::SolvableSpec.
   * It is intentionally a \b pure data container: it stores sets of
   * \ref IdString idents and \ref Capability provides tokens, but it holds
   * \b no pool reference, no cache, and no lazy-evaluation state.
   *
   * Evaluation (i.e., resolving which concrete \ref Solvable objects match)
   * is done externally by callers who supply an explicit \c Pool& reference.
   * This makes \c SolvableSpec safe to copy, store, and pass across
   * component boundaries without pool-lifetime concerns.
   *
   * \par Contrast with legacy
   * The legacy \c zypp::sat::SolvableSpec stores a lazy \c shared_ptr<WhatProvides>
   * cache and calls \c setDirty() to invalidate it.  That design couples the
   * spec to a specific pool instance.  Here the spec is stateless; the
   * \ref PackagePolicyComponent owns the cache and rebuilds it in \c prepare().
   *
   * \note This type does \b not inherit \ref PoolMember. It must never call
   *       \c myPool() or touch any global pool state.
   */
  class SolvableSpec
  {
  public:
    SolvableSpec()  = default;
    ~SolvableSpec() = default;

    SolvableSpec( const SolvableSpec & ) = default;
    SolvableSpec( SolvableSpec && )      = default;
    SolvableSpec & operator=( const SolvableSpec & ) = default;
    SolvableSpec & operator=( SolvableSpec && )      = default;

    /** Whether the spec has no idents and no provides tokens. */
    bool empty() const
    { return _idents.empty() && _provides.empty(); }

    /** Reset to empty state. */
    void clear()
    { _idents.clear(); _provides.clear(); }

    /** \name Ident-based matching
     *
     * A solvable matches by ident when its \c ident() IdString is in this set.
     */
    //@{
    /** Add \a ident_r to the ident set. No-op if \a ident_r is empty. */
    void addIdent( IdString ident_r )
    { if ( !ident_r.empty() ) _idents.insert( std::move(ident_r) ); }

    const IdStringSet & idents() const
    { return _idents; }

    /** Whether \a ident_r has been added to the spec (for introspection/tests). */
    bool containsIdent( const IdString & ident_r ) const
    { return _idents.count( ident_r ) != 0; }
    //@}

    /** \name Provides-based matching
     *
     * A solvable matches by provides when its provides list contains a
     * capability that matches one of these tokens.  Callers use
     * \c pool_whatprovides() to expand these tokens into a solvable set.
     */
    //@{
    /** Add \a cap_r to the provides set. No-op if \a cap_r is empty. */
    void addProvides( Capability cap_r )
    { if ( !cap_r.empty() ) _provides.insert( std::move(cap_r) ); }

    const CapabilitySet & provides() const
    { return _provides; }

    /** Whether \a cap_r has been added to the spec (for introspection/tests). */
    bool containsProvides( const Capability & cap_r ) const
    { return _provides.count( cap_r ) != 0; }
    //@}

    /** \name Text-format parser
     *
     * Each entry is either:
     * - \c "provides:CAPABILITY"  — parsed into \ref addProvides()
     * - anything else             — parsed into \ref addIdent()
     *
     * Empty strings and strings beginning with \c '#' are silently ignored.
     */
    //@{
    /** Parse and add a single spec entry. */
    void parse( std::string_view spec_r );

    /** Parse and add specs from \a istr_r (one per line; \c #-comments and
     *  empty lines are skipped automatically). */
    void parseFrom( const zypp::InputStream & istr_r );

    /** Parse and add specs from an iterator range of string-like values. */
    template <class TIterator>
    void parseFrom( TIterator begin, TIterator end )
    { for ( ; begin != end; ++begin ) parse( *begin ); }

    /** Split \a multispec_r on \c ',', \c ' ', \c '\\t' and parse each token. */
    void splitParseFrom( std::string_view multispec_r );
    //@}

    /**
     * \brief Test whether a single \ref Solvable matches this spec.
     *
     * A solvable matches if its ident is in \ref idents(), \b or if it
     * provides at least one of the capabilities in \ref provides().
     *
     * \param pool_r  The \ref Pool owning \a solv_r (used to expand
     *                provides tokens via \c whatprovides).
     * \param solv_r  The solvable to test.
     */
    bool contains( Pool & pool_r, const Solvable & solv_r ) const;

  private:
    IdStringSet    _idents;
    CapabilitySet  _provides;
  };

  /**
   * \class EvaluatedSolvableSpec
   * \brief A \ref SolvableSpec that has been evaluated against a specific \ref Pool.
   *
   * \par Design
   * This is the explicit, pool-bound counterpart to the pure-data \ref SolvableSpec.
   * Construction takes an explicit \c Pool& and immediately resolves the spec into
   * a flat \c SolvableIdType hash set.  Subsequent \ref contains() calls are O(1)
   * with no pool access required.
   *
   * The pool-binding is visible in the type: an \c EvaluatedSolvableSpec is a
   * snapshot of the pool at construction time.  If the pool changes, construct
   * a new instance.  There is no \c setDirty() / lazy-rebuild mechanism by design.
   *
   * \par Relationship to SolvableSpec
   * \code
   *   SolvableSpec spec;
   *   spec.parse( "provides:kernel-default" );
   *   // ... later, with a pool in hand:
   *   EvaluatedSolvableSpec eval( pool, spec );
   *   if ( eval.contains( solv ) ) { ... }   // O(1), no pool needed
   * \endcode
   */
  class EvaluatedSolvableSpec
  {
  public:
    using SolvIdSet = std::unordered_set<detail::SolvableIdType>;

    /** Construct by evaluating \a spec_r against \a pool_r. */
    EvaluatedSolvableSpec( Pool & pool_r, const SolvableSpec & spec_r );

    EvaluatedSolvableSpec( const EvaluatedSolvableSpec & ) = default;
    EvaluatedSolvableSpec( EvaluatedSolvableSpec && )      = default;
    EvaluatedSolvableSpec & operator=( const EvaluatedSolvableSpec & ) = default;
    EvaluatedSolvableSpec & operator=( EvaluatedSolvableSpec && )      = default;

    /** Whether no solvable matched the spec at construction time. */
    bool empty() const
    { return _ids.empty(); }

    /** O(1) membership test — no pool access required. */
    bool contains( const Solvable & solv_r ) const;

    /** Direct access to the resolved id set (e.g. for bulk operations). */
    const SolvIdSet & ids() const
    { return _ids; }

  private:
    SolvIdSet _ids;
  };

} // namespace zyppng::sat

#endif // ZYPP_NG_SAT_SOLVABLESPEC_H_INCLUDED
