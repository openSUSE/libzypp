/*-----------------------------------------------------------*- c++ -*-\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPPER_TABULATOR_H
#define ZYPPER_TABULATOR_H

#include <iostream>
#include <sstream>
#include <string>
#include <iosfwd>
#include <set>
#include <list>
#include <vector>

#include <zypp/base/String.h>

#include "main.h"
#include "utils/ansi.h"
#include "utils/colors.h"

//! table drawing style
enum TableLineStyle {
  Ascii		= 0,	///< | - +
  Light,
  Heavy,
  Double,
  Light3,
  Heavy3,
  LightHeavy,
  LightDouble,
  HeavyLight,
  DoubleLight,
  Colon,
  none,
  TLS_End,		///< sentinel
};

class Table;

class TableRow
{
private:
  std::ostream & dumpDetails( std::ostream & stream, const Table & parent ) const;

public:
  TableRow()
  : _ctxt( ColorContext::DEFAULT )
  {}

  explicit TableRow( unsigned c )
  : _ctxt( ColorContext::DEFAULT )
  { _columns.reserve(c); }

  explicit TableRow( ColorContext ctxt_r )
  : _ctxt( ctxt_r )
  {}

  TableRow( unsigned c, ColorContext ctxt_r  )
  : _ctxt( ctxt_r )
  { _columns.reserve(c); }

  TableRow & add( std::string s );

  template<class Tp_>
  TableRow & add( const Tp_ & val_r )
  { return add( asString( val_r ) ); }


  TableRow & addDetail( std::string s );

  template<class Tp_>
  TableRow & addDetail( const Tp_ & val_r )
  { return addDetail( asString( val_r ) ); }


  bool empty() const
  { return _columns.empty(); }

  // return number of columns
  unsigned size() const
  { return _columns.size(); }

  unsigned cols() const
  { return size(); }

  //! tab separated output
  std::ostream & dumbDumpTo( std::ostream & stream ) const;
  //! output with \a parent table attributes
  std::ostream & dumpTo( std::ostream & stream, const Table & parent ) const;

  typedef std::vector<std::string> container;

  int nsidx() const
  { return _nsidx; }

  void nsidx( int n )
  { _nsidx = n; }

  // BinaryPredicate
  struct Less
  {
    unsigned _by_column;
    Less( unsigned by_column ) : _by_column (by_column) {}

    bool operator()( const TableRow & a, const TableRow & b ) const
    {
      bool noL = _by_column >= a._columns.size();
      bool noR = _by_column >= b._columns.size();
      if ( noL || noR )
      {
	if ( noL && noR )
	  return a.nsidx() < b.nsidx();
	else
	  return noL && ! noR;
      }
      return a._columns[_by_column] < b._columns[_by_column];
    }
  };

  const container & columns() const
  { return _columns; }

  container & columns()
  { return _columns; }

private:
  container _columns;
  container _details;
  ColorContext _ctxt;
  ZeroInit<int>_nsidx;	///< numerial sort index, e.g. if string values don't work due to coloring
  friend class Table;
};

/** \relates TableRow Add colummn. */
template<class Tp_>
TableRow & operator<<( TableRow & tr, Tp_ && val )
{ return tr.add( asString( std::forward<Tp_>(val) ) ); }
/** \overload preserving TableRow rvalue reference. */
template<class Tp_>
TableRow && operator<<( TableRow && tr, Tp_ && val )
{ return std::move( tr << std::forward<Tp_>(val) ); }


class TableHeader : public TableRow {
public:
  //! Constructor. Reserve place for c columns.
  TableHeader (unsigned c = 0): TableRow (c) {}
};

/** \relates TableHeader  Add colummn. */
template<class Tp_>
TableHeader & operator<<( TableHeader & th, Tp_ && val )
{ static_cast<TableRow&>(th) << std::forward<Tp_>(val); return th; }
/** \overload preserving TableHeader rvalue reference. */
template<class Tp_>
TableHeader && operator<<( TableHeader && th, Tp_ && val )
{ return std::move( th << std::forward<Tp_>(val) ); }


/** \todo nice idea but poor interface */
class Table
{
public:
  typedef std::list<TableRow> container;

  static TableLineStyle defaultStyle;

  Table & add( TableRow tr );

  Table & setHeader( TableHeader tr );


  std::ostream & dumpTo( std::ostream & stream ) const;
  bool empty() const { return _rows.empty(); }


  /** Unsorted - pseudo sort column indicating not to sort. */
  static constexpr unsigned Unsorted		= unsigned(-1);
  /** Nsidx - pseudo sort column using the numberical sort index. */
  static constexpr unsigned Nsidx		= unsigned(-2);

  /** Get the default sort column or \ref Unsorted (default) */
  unsigned defaultSortColumn() const		{ return _defaultSortColumn; }

  /** Set a \ref defaultSortColumn */
  void defaultSortColumn( unsigned byColumn_r )	{ _defaultSortColumn = byColumn_r; }

  /** Sort by \ref defaultSortColumn */
  void sort()					{ sort( unsigned(_defaultSortColumn ) ); }

  /** Sort by \a byColumn_r */
  void sort( unsigned byColumn_r )		{ if ( byColumn_r != Unsorted ) _rows.sort( TableRow::Less( byColumn_r ) ); }

  /** Custom sort */
  template<class TCompare, typename std::enable_if<!std::is_integral<TCompare>::value>::type* = nullptr>
  void sort( TCompare && less_r )		{ _rows.sort( std::forward<TCompare>(less_r) ); }

  void lineStyle( TableLineStyle st );
  void wrap( int force_break_after = -1 );
  void allowAbbrev( unsigned column );
  void margin( unsigned margin );

  const TableHeader & header() const
  { return _header; }
  const container & rows() const
  { return _rows; }
  container & rows()
  { return _rows; }

  Table();

  // poor workaroud missing column styles and table entry objects
  void setEditionStyle( unsigned column )
  { _editionStyle.insert( column ); }

private:
  void dumpRule( std::ostream & stream ) const;
  void updateColWidths( const TableRow & tr ) const;

  bool _has_header;
  TableHeader _header;
  container _rows;

  //! maximum column index seen in this table
  mutable unsigned _max_col;
  //! maximum width of respective columns
  mutable std::vector<unsigned> _max_width;
  //! table width (columns)
  mutable int _width;
  //! table line drawing style
  TableLineStyle _style;
  //! amount of space we have to print this table
  int _screen_width;
  //! whether to abbreviate the respective column if needed
  std::vector<bool> _abbrev_col;
  //! left/right margin in number of spaces
  unsigned _margin;
  //! if _do_wrap is set, first break the table at this column;
  //! If negative, wrap as needed.
  int _force_break_after;
  //! Whether to wrap the table if it exceeds _screen_width
  bool _do_wrap;

  DefaultIntegral<unsigned,Unsorted> _defaultSortColumn;

  mutable bool _inHeader;
  std::set<unsigned> _editionStyle;
  bool editionStyle( unsigned column ) const
  { return _editionStyle.find( column ) != _editionStyle.end(); }

  friend class TableRow;
};

namespace table
{
  /** TableHeader manipulator */
  struct EditionStyleSetter
  {
    EditionStyleSetter( Table & table_r, std::string header_r )
    : _table( table_r )
    , _header( std::move(header_r) )
    {}
    Table & _table;
    std::string _header;
  };

  // NOTE: Overloading TableHeader operator<< which uses a universal-reference
  // requires excact matches for the EditionStyleSetter arg! Missing overloads
  // result in compiler complaining about missing EditionStyleSetter::asString.

  /** \relates table::EditionStyleSetter */
  inline TableHeader & operator<<( TableHeader & th, EditionStyleSetter && obj )
  {
    obj._table.setEditionStyle( th.cols() );
    th.add( std::move(obj._header) );
    return th;
  }
  /** \overload preserving TableHeader rvalue reference. */
  inline TableHeader && operator<<( TableHeader && th, EditionStyleSetter && obj )
  { return std::move( th << std::move(obj) ); }
}


inline Table & operator<<( Table & table, TableRow tr )
{ return table.add( std::move(tr) ); }

inline Table & operator<<( Table & table, TableHeader tr )
{ return table.setHeader( std::move(tr) ); }


inline std::ostream & operator<<( std::ostream & stream, const Table & table )
{ return table.dumpTo( stream ); }


///////////////////////////////////////////////////////////////////
/// \class PropertyTable
/// \brief Alligned key/value with multiline support
/// Key       : value 1
/// LongKey   : value 2
/// Multiline :
///     line 1
///     line 2
/// Next Key  : value 3
///
///////////////////////////////////////////////////////////////////
class PropertyTable
{
public:
  PropertyTable()
  { _table.lineStyle( ::Colon ); }

  static const char * emptyListTag() { return "---"; }

public:
  ///////////////////////////////////////////////////////////////////
  // Key / Value
  template <class KeyType>
  PropertyTable & add( const KeyType & key_r )
  { _table << ( TableRow() << key_r << "" ); return *this; }

  template <class KeyType, class ValueType>
  PropertyTable & add( const KeyType & key_r, const ValueType & val_r )
  { _table << ( TableRow() << key_r << val_r ); return *this; }

  template <class KeyType>
  PropertyTable & add( const KeyType & key_r, bool val_r )
  { _table << ( TableRow() << key_r << asYesNo( val_r ) ); return *this; }

  ///////////////////////////////////////////////////////////////////
  // Key / Value in details (e.g. Description:)
  template <class ValueType>
  PropertyTable & addDetail( const ValueType & val_r )
  { last().addDetail( val_r ); return *this; }

  template <class KeyType, class ValueType>
  PropertyTable & addDetail( const KeyType & key_r, const ValueType & val_r )
  { _table << ( TableRow() << key_r << "" ).addDetail( val_r ); return *this; }

  ///////////////////////////////////////////////////////////////////
  // Key / Container<Value>
  template <class KeyType, class Iterator_ >
  PropertyTable & add( const KeyType & key_r, Iterator_ begin_r, Iterator_ end_r, bool forceDetails_r = false  )
  {
    TableRow r;
    r << key_r;
    if ( begin_r != end_r )
    {
      unsigned cnt = 1;
      Iterator_ first = begin_r++;
      if ( begin_r == end_r && ! forceDetails_r )
	r << *first;				// only one value
      else
      {
	r.addDetail( *first );			// list all in details
	while ( begin_r != end_r )
	{
	  ++cnt;
	  r.addDetail( *(begin_r++) );
	}
	r << "["+str::numstring(cnt)+"]";	// size as value
      }
   }
    else
    { r << emptyListTag(); }				// dummy to get the ":" if empty
    _table << r;
    return *this;
  }

  template <class KeyType, class ContainerType>
  PropertyTable & lst( const KeyType & key_r, const ContainerType & lst_r, bool forceDetails_r = false )
  { return add( key_r, lst_r.begin(), lst_r.end(), forceDetails_r ); }

  template <class KeyType, class ValueType>
  PropertyTable & add( const KeyType & key_r, const std::set<ValueType> & lst_r, bool forceDetails_r = false  )
  { return lst( key_r, lst_r, forceDetails_r );  }
  template <class KeyType, class ValueType>
  PropertyTable & add( const KeyType & key_r, const std::list<ValueType> & lst_r, bool forceDetails_r = false  )
  { return lst( key_r, lst_r, forceDetails_r );  }
  template <class KeyType, class ValueType>
  PropertyTable & add( const KeyType & key_r, const std::vector<ValueType> & lst_r, bool forceDetails_r = false  )
  { return lst( key_r, lst_r, forceDetails_r ); }

  ///////////////////////////////////////////////////////////////////
  // misc
  PropertyTable & paint( ansi::Color color_r, bool cond_r = true )
  {
    if ( cond_r )
    {
      // FIXME re-coloring like this works ony once
      std::string & lastval( _table.rows().back().columns().back() );
      lastval = ColorString( lastval, color_r ).str();
    }
    return *this;
  }

  TableRow & last()
  { return _table.rows().back(); }

  std::string & lastKey()
  { return last().columns()[0]; }

  std::string & lastValue()
  { return last().columns()[1]; }

public:
  friend std::ostream & operator << ( std::ostream & str, const PropertyTable & obj )
  { return str << obj._table; }

private:
  Table _table;
};



// Local Variables:
// c-basic-offset: 2
// End:
#endif
