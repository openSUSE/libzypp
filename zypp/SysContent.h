/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/SysContent.h
 *
*/
#ifndef ZYPP_SYSCONTENT_H
#define ZYPP_SYSCONTENT_H

#include <iosfwd>
#include <string>
#include <set>

#include "zypp/PoolItem.h"
#include "zypp/Edition.h"
#include "zypp/Date.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace syscontent
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : Writer
    //
    /** Collect and serialize a set of \ref ResObject.
     * \code
     * <?xml version="1.0" encoding="UTF-8"?>
     * <syscontent>
     *   <ident>
     *     <name>mycollection</name>
     *     <version epoch="0" ver="1.0" rel="1"/>
     *     <description>All the cool stuff...</description>
     *     <created>1165270942</created>
     *   </ident>
     *   <onsys>
     *     <entry kind="package" name="pax" epoch="0" ver="3.4" rel="12" arch="x86_64"/>
     *     <entry kind="product" name="SUSE_SLES" epoch="0" ver="10" arch="x86_64"/>
     *     <entry ...
     *   </onsys>
     * </syscontent>
     * \endcode
    */
    class Writer
    {
      typedef std::set<ResObject::constPtr> StorageT;
    public:
      typedef StorageT::value_type     value_type;
      typedef StorageT::size_type      size_type;
      typedef StorageT::iterator       iterator;
      typedef StorageT::const_iterator const_iterator;

    public:
      /** \name Identification.
       * User provided optional data to identify the collection.
      */
      //@{
      /** Get name. */
      const std::string & name() const
      { return _name; }

      /** Set name. */
      Writer & name( const std::string & val_r )
      { _name = val_r; return *this; }

      /** Get edition. */
      const Edition & edition() const
      { return _edition; }

      /** Set edition. */
      Writer & edition( const Edition & val_r )
      { _edition = val_r; return *this; }

      /** Get description. */
      const std::string & description() const
      { return _description; }

      /** Set description.*/
      Writer & description( const std::string & val_r )
      { _description = val_r; return *this; }
      //@}

    public:
      /** \name Collecting data.
       * \code
       * syscontent::Writer contentW;
       * contentW.name( "mycollection" )
       *         .edition( Edition( "1.0" ) )
       *         .description( "All the cool stuff..." );
       *
       * ResPool pool( getZYpp()->pool() );
       * for_each( pool.begin(), pool.end(),
       *           bind( &syscontent::Writer::addIf, ref(contentW), _1 ) );
       *
       * std::ofstream my_file( "some_file" );
       * my_file << contentW;
       * my_file.close();
       * \endcode
      */
      //@{
      /** Collect currently installed \ref PoolItem. */
      void addInstalled( const PoolItem & obj_r )
      {
        if ( obj_r.status().isInstalled() )
          {
            _onsys.insert( obj_r.resolvable() );
          }
      }

      /** Collect \ref PoolItem if it stays on the system.
       * I.e. it stays installed or is tagged to be installed.
       * Solver selected items are omitted.
      */
      void addIf( const PoolItem & obj_r )
      {
        if ( obj_r.status().isInstalled() != obj_r.status().transacts()
             && ! ( obj_r.status().transacts() && obj_r.status().isBySolver() ) )
          {
            _onsys.insert( obj_r.resolvable() );
          }
      }

      /** Unconditionally add this \ref ResObject (or \ref PoolItem). */
      void add( const ResObject::constPtr & obj_r )
      { _onsys.insert( obj_r ); }
      //@}

    public:
      /** \name Collected data. */
      //@{
      /** Whether no data collected so far. */
      bool empty() const
      { return _onsys.empty(); }

      /** Number of items collected. */
      size_type size() const
      { return _onsys.size(); }

      /** Iterator to the begin of collected data. */
      const_iterator begin() const
      { return _onsys.begin(); }

      /** Iterator to the end of collected data. */
      const_iterator end() const
      { return _onsys.end(); }
      //@}

    public:
      /** Write collected data as XML.
       * Read them back using \ref Reader.
      */
      std::ostream & writeXml( std::ostream & str ) const;

    private:
      std::string _name;
      Edition     _edition;
      std::string _description;
      StorageT    _onsys;
    };
    ///////////////////////////////////////////////////////////////////

    /** \relates Writer Stream output */
    inline std::ostream & operator<<( std::ostream & str, const Writer & obj )
    { return obj.writeXml( str ); }

    /////////////////////////////////////////////////////////////////
  } // namespace syscontent
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_SYSCONTENT_H
