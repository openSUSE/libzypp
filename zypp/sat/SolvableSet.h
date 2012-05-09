/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/SolvableSet.h
 *
*/
#ifndef ZYPP_SAT_SOLVABLESET_H
#define ZYPP_SAT_SOLVABLESET_H

#include <iosfwd>

#include "zypp/base/Macros.h"
#include "zypp/base/PtrTypes.h"
#include "zypp/base/Tr1hash.h"
#include "zypp/sat/Solvable.h"
#include "zypp/sat/SolvIterMixin.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace sat
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : SolvableSet
    //
    /** Solvable set wrapper to allow adding additioanal convenience iterators.
     */
    class ZYPP_API SolvableSet : public SolvIterMixin<SolvableSet,std::tr1::unordered_set<Solvable>::const_iterator>
    {
      friend std::ostream & operator<<( std::ostream & str, const SolvableSet & obj );

      public:
        typedef std::tr1::unordered_set<Solvable> Container;
        typedef Container::value_type             value_type;
        typedef Container::size_type              size_type;
        typedef Solvable_iterator                 const_iterator; // from SolvIterMixin

      public:
        /** Default ctor */
        SolvableSet()
        : _pimpl( new Container )
        {}

        /** Ctor building a set from a range. */
        template<class _InputIterator>
        SolvableSet( _InputIterator begin_r, _InputIterator end_r )
        : _pimpl( new Container( begin_r, end_r ) )
        {}

      public:
        /** Whether the set is epmty. */
        bool empty() const
        { return _pimpl->empty(); }

        /** Size of the set. */
        size_type size() const
        { return _pimpl->size(); }

	/** */
	template<class _Solv>
	bool contains( const _Solv & solv_r ) const
	{ return( get().find( asSolvable()( solv_r ) ) != end() ); }

        /** Iterator pointing to the first \ref Solvable. */
        const_iterator begin() const
        { return _pimpl->begin(); }

        /** Iterator pointing behind the last \ref Solvable. */
        const_iterator end() const
        { return _pimpl->end(); }

      public:

	/** Insert a Solvable.
	 * \return \c true if it was actually inserted, or \c false if already present.
	*/
	template<class _Solv>
	bool insert( const _Solv & solv_r )
	{ return get().insert( asSolvable()( solv_r ) ).second; }

      public:
        /** The set. */
        Container & get()
        { return *_pimpl; }

        /** The set. */
        const Container & get() const
        { return *_pimpl; }

      private:
        /** Pointer to implementation */
        RWCOW_pointer<Container> _pimpl;
    };
    ///////////////////////////////////////////////////////////////////

    /** \relates SolvableSet Stream output */
    std::ostream & operator<<( std::ostream & str, const SolvableSet & obj );

    /////////////////////////////////////////////////////////////////
  } // namespace sat
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_SAT_SOLVABLESET_H
