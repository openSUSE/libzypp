/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/Capabilities.h
 *
*/
#ifndef ZYPP_NG_SAT_CAPABILITIES_H
#define ZYPP_NG_SAT_CAPABILITIES_H

#include <iosfwd>
#include <iterator>
#include <type_traits>

#include <zypp-core/base/DefaultIntegral>
#include <zypp/ng/sat/capability.h>

namespace zyppng::sat
{
  /**
   * \class Capabilities
   * \brief Container of \ref Capability (currently read only).
   *
   * \note libsolv dependency lists may include internal ids
   * which must be skipped on iteration or size calculation
   * (\see \ref detail::isDepMarkerId).
   */
  class Capabilities
  {
    public:
      typedef Capability value_type;
      typedef unsigned   size_type;

      enum Mode { SKIP_TO_INTERNAL };

    public:
      /** Default ctor */
      Capabilities()
      : _begin( 0 )
      {}

      /** Ctor from Id pointer (friend \ref Solvable). */
      explicit
      Capabilities( const sat::detail::IdType * base_r )
      : _begin( base_r )
      {}

      /** Ctor from Id pointer (friend \ref Solvable).
       * Jump behind skip_r (e.g. behind prereqMarker).
       */
      Capabilities( const sat::detail::IdType * base_r, sat::detail::IdType skip_r );

    public:
      /** Whether the container is empty. */
      bool empty() const
      { return ! ( _begin && *_begin ); }

      /** Number of capabilities inside. */
      size_type size() const;

    public:
      class const_iterator;

      /** Iterator pointing to the first \ref Capability. */
      const_iterator begin() const;

      /** Iterator pointing behind the last \ref Capability. */
      const_iterator end() const;

  public:
    /** Return whether the set contains \a lhs (the Id)*/
    bool contains( const Capability & lhs ) const;

    /** Return whether \a lhs matches at least one capability in set. */
    bool matches( const Capability & lhs ) const;

    /** Return the first matching Capability in set or Capability::Null. */
    Capability findFirstMatch( const Capability & lhs ) const;

    private:
      const sat::detail::IdType * _begin;
  };

  /**
   *  \class Capabilities::const_iterator
   *  \ref Capabilities iterator.
   *
   *  A forward iterator over a libsolv id array. Automatically skips
   *  internal dep-marker ids, setting the \ref tagged flag when it does so
   *  to indicate that subsequent capabilities carry a special property
   *  (e.g. pre-requires within a solvable's requires list).
   */
  class Capabilities::const_iterator
  {
    public:
      using iterator_category = std::forward_iterator_tag;
      using value_type        = Capability;
      using difference_type   = std::ptrdiff_t;
      using pointer           = void;        // proxy iterator — no stable address
      using reference         = const Capability;

      const_iterator()
      : _idx( nullptr ), _tagged( false )
      {}

      explicit const_iterator( const sat::detail::IdType * idx )
      : _idx( idx ), _tagged( false )
      {
        // Skip an initial dep-marker if present, tagging subsequent entries.
        if ( _idx && sat::detail::isDepMarkerId( *_idx ) )
        {
          _tagged = true;
          ++_idx;
        }
      }

    public:
      /** Return \c true if the \ref Capability is \c tagged.
       * The meaning of \c tagged depends on the kind of dependency you
       * are processing. It is a hint that the iterator skipped some
       * internal marker, indicating that subsequent capabilities have
       * a special property. Within a \ref Solvable's requirements e.g.
       * the pre-requirements are tagged.
       * \code
       * Capabilities req( solvable.dep_requires() );
       * for ( auto it = req.begin(); it != req.end(); ++it )
       * {
       *   if ( it.tagged() )
       *     cout << *it << " (is prereq)" << endl;
       *   else
       *     cout << *it << endl;
       * }
       * \endcode
      */
      bool tagged() const { return _tagged; }

      reference operator*() const
      { return _idx ? Capability( *_idx ) : Capability::Null; }

      const_iterator & operator++()
      {
        // Advance and jump over any libsolv internal dep-marker ids.
        if ( sat::detail::isDepMarkerId( *(++_idx) ) )
        {
          _tagged = true;
          ++_idx;
        }
        return *this;
      }

      const_iterator operator++(int)
      {
        const_iterator tmp( *this );
        ++(*this);
        return tmp;
      }

      bool operator==( const const_iterator & rhs ) const
      {
        // A NULL pointer is equal to a pointer pointing at Id 0 (end sentinel).
        return ( _idx == rhs._idx )
            || ( !rhs._idx && _idx    && !*_idx     )
            || ( !_idx     && rhs._idx && !*rhs._idx );
      }

      bool operator!=( const const_iterator & rhs ) const
      { return !( *this == rhs ); }

    private:
      const sat::detail::IdType *       _idx;
      zypp::DefaultIntegral<bool,false> _tagged;
  };
  ///////////////////////////////////////////////////////////////////

  inline Capabilities::const_iterator Capabilities::begin() const
  { return const_iterator( _begin ); }

  inline Capabilities::const_iterator Capabilities::end() const
  { return const_iterator( 0 ); }

  inline bool Capabilities::contains( const Capability & lhs ) const
  {
    for ( const Capability & rhs : *this )
      if ( lhs == rhs )
        return true;
    return false;
  }

  inline bool Capabilities::matches( const Capability & lhs ) const
  {
    for ( const Capability & rhs : *this )
      if ( lhs.matches( rhs ) == CapMatch::yes )
        return true;
    return false;
  }

  inline Capability Capabilities::findFirstMatch( const Capability & lhs ) const
  {
    for ( const Capability & rhs : *this )
      if ( lhs.matches( rhs ) == CapMatch::yes )
        return rhs;
    return Capability::Null;
  }
} // namespace zyppng::sat
#endif // ZYPP_NG_SAT_CAPABILITIES_H
