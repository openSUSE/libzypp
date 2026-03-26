/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-core/ng/base/iterators.h
 *
*/
#ifndef ZYPP_CORE_NG_BASE_ITERATORS_H_INCLUDED
#define ZYPP_CORE_NG_BASE_ITERATORS_H_INCLUDED

#include <iterator>
#include <utility>

namespace zyppng {

  /**
   * \brief A simple forward iterator that filters a base range according to a predicate.
   *
   * This class serves as a C++17 compatible drop-in replacement for `boost::filter_iterator`.
   * It is designed as a stopgap measure to eliminate Boost dependencies in the Next-Generation (NG)
   * layer until full C++20 `std::ranges` or a polyfill like `range-v3` is integrated.
   *
   * The iterator takes a predicate (which returns true for elements that should be included)
   * and a pair of iterators (`it` and `end`) representing the underlying sequence.
   * Whenever incremented, it automatically advances until the predicate is satisfied or
   * the `end` boundary is reached.
   *
   * \note Because this is a C++17 forward iterator and not a C++20 sentinel-based view,
   * it must carry the `_end` iterator internally, making it a "fat iterator".
   *
   * \tparam Pred A callable that takes a value of the base sequence and returns `bool`.
   * \tparam Base The underlying forward iterator type.
   */
  template<class Pred, class Base>
  class FilterIterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = typename std::iterator_traits<Base>::value_type;
    using difference_type   = typename std::iterator_traits<Base>::difference_type;
    using pointer           = typename std::iterator_traits<Base>::pointer;
    using reference         = typename std::iterator_traits<Base>::reference;

    /** \brief Default constructor. Creates an invalid/end iterator. */
    FilterIterator() = default;

    /**
     * \brief Constructs a FilterIterator.
     * \param p The predicate used to filter elements.
     * \param it The start of the base sequence.
     * \param end The end of the base sequence.
     */
    FilterIterator(Pred p, Base it, Base end) 
      : _pred(std::move(p)), _it(std::move(it)), _end(std::move(end)) 
    {
      satisfy_predicate();
    }

    /** \brief Dereference operator. */
    reference operator*() const { return *_it; }
    
    /** \brief Arrow operator. */
    pointer operator->() const { return &(*_it); }

    /** \brief Pre-increment operator. Advances to the next element satisfying the predicate. */
    FilterIterator& operator++() {
      ++_it;
      satisfy_predicate();
      return *this;
    }

    /** \brief Post-increment operator. */
    FilterIterator operator++(int) {
      FilterIterator tmp = *this;
      ++(*this);
      return tmp;
    }

    /** \brief Equality comparison. */
    bool operator==(const FilterIterator& other) const {
      return _it == other._it;
    }

    /** \brief Inequality comparison. */
    bool operator!=(const FilterIterator& other) const {
      return !(*this == other);
    }

    /** \brief Returns the underlying base iterator at its current position. */
    Base const & base() const { return _it; }

  private:
    /** \brief Advances the internal iterator until the predicate is met or `_end` is reached. */
    void satisfy_predicate() {
      while (_it != _end && !_pred(*_it)) {
        ++_it;
      }
    }

    Pred _pred;
    Base _it;
    Base _end;
  };

  /**
   * \brief Helper function to deduce types and construct a \ref FilterIterator.
   * \param p The predicate.
   * \param it The start of the base sequence.
   * \param end The end of the base sequence.
   * \return A newly constructed \ref FilterIterator.
   */
  template<class Pred, class Base>
  FilterIterator<Pred, Base> make_filter_iterator(Pred p, Base it, Base end) {
    return FilterIterator<Pred, Base>(std::move(p), std::move(it), std::move(end));
  }

} // namespace zyppng

#endif
