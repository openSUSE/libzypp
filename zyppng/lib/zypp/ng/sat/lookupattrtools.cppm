/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
module;
#include <cstddef>
#include <iterator>
#include <ranges>
#include <utility>

export module zyppng:sat_lookupattrtools;

import :sat_lookupattr;
import :sat_solvattr;
import :sat_repository;
import :sat_solvable;

export namespace zyppng::sat {

  ///////////////////////////////////////////////////////////////////
  //
  //  CLASS NAME : LookupAttr::TransformIterator
  //
  /**
   * \brief A C++20 forward iterator adaptor over \ref LookupAttr::iterator.
   *
   * Replaces the legacy Boost-based \c LookupAttr::TransformIterator.
   *
   * On each dereference, the underlying \ref LookupAttrValue proxy is
   * retrieved and \c TAttr is extracted via \c asType<TAttr>(); the
   * returned value is then \c TResult{ extracted_value }.
   *
   * \tparam TResult  The value type exposed to the caller.
   * \tparam TAttr    The intermediate type retrieved from the pool via
   *                  \c LookupAttrValue::asType<TAttr>().
   *                  Defaults to \c TResult (identity transform).
   *
   * Satisfies \c std::forward_iterator.
   *
   * \note Lifetime: the iterator is valid only as long as the originating
   * \ref LookupAttr query object is alive — same rules as
   * \ref LookupAttr::iterator.
   */
  template<class TResult, class TAttr = TResult>
  class LookupAttr::TransformIterator
  {
  public:
    // ── iterator_traits ────────────────────────────────────────────
    using iterator_category = std::forward_iterator_tag;
    using value_type        = TResult;
    using difference_type   = std::ptrdiff_t;
    using pointer           = void;    ///< proxy — no stable address
    using reference         = TResult; ///< returned by value

    // ── Construction ───────────────────────────────────────────────
    TransformIterator() = default;

    explicit TransformIterator( const LookupAttr::iterator & base )
    : _base( base )
    {}

    // ── Dereference ────────────────────────────────────────────────
    /** Retrieve the current value as \c TResult. */
    reference operator*() const
    {
      // (*_base) returns a LookupAttrValue proxy; asType<TAttr>() extracts
      // the raw attribute value; TResult is then constructed from it.
      return TResult( (*_base).asType<TAttr>() );
    }

    // ── Increment ──────────────────────────────────────────────────
    TransformIterator & operator++()
    { ++_base; return *this; }

    TransformIterator operator++( int )
    { auto tmp = *this; ++_base; return tmp; }

    // ── Equality ───────────────────────────────────────────────────
    bool operator==( const TransformIterator & rhs ) const
    { return _base == rhs._base; }

    bool operator!=( const TransformIterator & rhs ) const
    { return !( *this == rhs ); }

    // ── Fast-forward skip methods (delegated to base iterator) ─────
    /** \name Moving fast forward. */
    //@{
    /** On the next \c operator++ advance to the next \ref SolvAttr. */
    void nextSkipSolvAttr()  { _base.nextSkipSolvAttr(); }

    /** On the next \c operator++ advance to the next \ref Solvable. */
    void nextSkipSolvable()  { _base.nextSkipSolvable(); }

    /** On the next \c operator++ advance to the next \ref Repository. */
    void nextSkipRepo()      { _base.nextSkipRepo(); }

    /** Immediately advance to the next \ref SolvAttr. */
    void skipSolvAttr()      { _base.skipSolvAttr(); }

    /** Immediately advance to the next \ref Solvable. */
    void skipSolvable()      { _base.skipSolvable(); }

    /** Immediately advance to the next \ref Repository. */
    void skipRepo()          { _base.skipRepo(); }
    //@}

    // ── Current position info (delegated to base iterator value) ───
    /** \name Current position info. */
    //@{
    /** The current \ref Repository. */
    Repository inRepo() const     { return (*_base).inRepo(); }

    /** The current \ref Solvable. */
    Solvable   inSolvable() const { return (*_base).inSolvable(); }

    /** The current \ref SolvAttr. */
    SolvAttr   inSolvAttr() const { return (*_base).inSolvAttr(); }
    //@}

    /** Expert backdoor — access the underlying base iterator. */
    const LookupAttr::iterator & base() const { return _base; }

  private:
    LookupAttr::iterator _base;
  };

  // Verify the concept is satisfied at definition time.
  // (static_assert inside a template is checked on instantiation; the
  //  explicit instantiation below forces the check for the common case.)
  static_assert( std::forward_iterator< LookupAttr::TransformIterator<IdString, IdString> > );

  ///////////////////////////////////////////////////////////////////
  //
  //  CLASS NAME : ArrayAttr
  //
  /**
   * \brief A \c std::ranges::input_range container for list-typed solvable
   * attributes, built on top of \ref LookupAttr::TransformIterator.
   *
   * Replaces the legacy Boost-based \c ArrayAttr.
   *
   * \code
   *   // Iterate all keywords of a solvable:
   *   ArrayAttr<PackageKeyword, IdString> kw( SolvAttr::keywords, solv );
   *   for ( const PackageKeyword & k : kw )
   *     MIL << k << '\n';
   * \endcode
   *
   * \tparam TResult  The value type exposed to the caller.
   * \tparam TAttr    The intermediate type retrieved from the pool.
   *
   * Satisfies \c std::ranges::forward_range.
   */
  template<class TResult, class TAttr = TResult>
  class ArrayAttr
  {
  public:
    using iterator  = LookupAttr::TransformIterator<TResult, TAttr>;
    using size_type = LookupAttr::size_type;

    // ── Construction ───────────────────────────────────────────────
    ArrayAttr() = default;

    /** Pool-wide attribute query. Pool is mandatory in the NG design — there
     *  is no global pool singleton. */
    explicit ArrayAttr( Pool & pool_r, SolvAttr attr_r,
                        LookupAttr::Location loc_r = LookupAttr::SOLV_ATTR )
    : _q( pool_r, std::move(attr_r), loc_r )
    {}

    /** Attribute query restricted to a single \ref Repository. */
    ArrayAttr( SolvAttr attr_r, Repository repo_r,
               LookupAttr::Location loc_r = LookupAttr::SOLV_ATTR )
    : _q( std::move(attr_r), repo_r, loc_r )
    {}

    /** Attribute query restricted to a single \ref Solvable. */
    ArrayAttr( SolvAttr attr_r, Solvable solv_r )
    : _q( std::move(attr_r), solv_r )
    {}

    // ── Range interface ────────────────────────────────────────────
    iterator begin() const { return iterator( _q.begin() ); }
    iterator end()   const { return iterator( _q.end() ); }

    bool empty() const { return _q.empty(); }

    /**
     * Number of results.
     * \note Not O(1) — runs the full query.
     */
    size_type size() const
    {
      size_type count = 0;
      for ( [[maybe_unused]] const auto & _ : *this )
        ++count;
      return count;
    }

    // ── Search ─────────────────────────────────────────────────────
    /** Find the first element equal to \a key_r, or \c end(). */
    iterator find( const TResult & key_r ) const
    {
      for ( auto it = begin(); it != end(); ++it )
        if ( *it == key_r )
          return it;
      return end();
    }

    // Stream output is provided by log::formatter<ArrayAttr<TResult,TAttr>>
    // in zyppng:log_sat_lookupattr — see log/sat/lookupattr.cppm.

  private:
    LookupAttr _q;
  };

  // Verify range concept for the common IdString case.
  static_assert( std::ranges::forward_range< ArrayAttr<IdString, IdString> > );

} // namespace zyppng::sat
