#include "Tools.h"

extern "C"
{
#include <libxml/xmlreader.h>
#include <libxml/xmlerror.h>
}
#include <iostream>
#include <fstream>
#include <list>
#include <map>
#include <set>

#include <boost/call_traits.hpp>

#include <zypp/base/LogControl.h>
#include <zypp/base/LogTools.h>

#include "zypp/base/Exception.h"
#include "zypp/base/InputStream.h"
#include "zypp/base/DefaultIntegral.h"
#include <zypp/base/Function.h>
#include <zypp/base/Iterator.h>
#include <zypp/Pathname.h>
#include <zypp/Edition.h>
#include <zypp/CheckSum.h>
#include <zypp/Date.h>
#include <zypp/Rel.h>

#include "zypp/xml/Reader.h"

#include "zypp/ZYppFactory.h"
#include "zypp/SysContent.h"


using namespace std;
using namespace zypp;

///////////////////////////////////////////////////////////////////

static const Pathname sysRoot( "/Local/ROOT" );

///////////////////////////////////////////////////////////////////

template<class _Cl>
  void ti( const _Cl & c )
  {
    SEC << __PRETTY_FUNCTION__ << endl;
  }

///////////////////////////////////////////////////////////////////

bool nopNode( xml::Reader & reader_r )
{
  return true;
}

///////////////////////////////////////////////////////////////////

bool accNode( xml::Reader & reader_r )
{
  int i;
  xml::XmlString s;
#define X(m) reader_r->m()
      i=X(readState);
      i=X(lineNumber);
      i=X(columnNumber);
      i=X(depth);
      i=X(nodeType);
      s=X(name);
      s=X(prefix);
      s=X(localName);
      i=X(hasAttributes);
      i=X(attributeCount);
      i=X(hasValue);
      s=X(value);
#undef X
      return true;
}

///////////////////////////////////////////////////////////////////

bool dumpNode( xml::Reader & reader_r )
{
  switch ( reader_r->nodeType() )
    {
    case XML_READER_TYPE_ATTRIBUTE:
    case XML_READER_TYPE_TEXT:
    case XML_READER_TYPE_CDATA:
       DBG << *reader_r << endl;
       break;
    case XML_READER_TYPE_ELEMENT:

       MIL << *reader_r << endl;
       break;
    default:
       //WAR << *reader_r << endl;
       break;
    }
  return true;
}

///////////////////////////////////////////////////////////////////

bool dumpEd( xml::Reader & reader_r )
{
  static int num = 5;
  if ( reader_r->nodeType() == XML_READER_TYPE_ELEMENT
       && reader_r->name() == "version" )
    {
      MIL << *reader_r << endl;
      DBG << reader_r->getAttribute( "rel" ) << endl;
      ERR << *reader_r << endl;
      DBG << reader_r->getAttribute( "ver" ) << endl;
      ERR << *reader_r << endl;
      DBG << reader_r->getAttribute( "epoch" ) << endl;
      ERR << *reader_r << endl;
      WAR << Edition( reader_r->getAttribute( "ver" ).asString(),
                      reader_r->getAttribute( "rel" ).asString(),
                      reader_r->getAttribute( "epoch" ).asString() ) << endl;
      --num;
    }
  return num;
}

///////////////////////////////////////////////////////////////////

template<class _OutputIterator>
  struct DumpDeps
  {
    DumpDeps( _OutputIterator result_r )
    : _result( result_r )
    {}

    bool operator()( xml::Reader & reader_r )
    {
      if ( reader_r->nodeType()     == XML_READER_TYPE_ELEMENT
           && reader_r->prefix()    == "rpm"
           && reader_r->localName() == "entry" )
        {
          string n( reader_r->getAttribute( "name" ).asString() );
          Rel op( reader_r->getAttribute( "flags" ).asString() );
          if ( op != Rel::ANY )
            {
              n += " ";
              n += op.asString();
              n += " ";
              n += reader_r->getAttribute( "ver" ).asString();
              n += "-";
              n += reader_r->getAttribute( "rel" ).asString();
            }
          *_result = n;
          ++_result;
        }
      return true;
    }

    _OutputIterator _result;
  };

template<class _OutputIterator>
  DumpDeps<_OutputIterator> dumpDeps( _OutputIterator result_r )
  { return DumpDeps<_OutputIterator>( result_r ); }

///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace syscontent
  { /////////////////////////////////////////////////////////////////

    class Reader
    {
    public:
      struct Entry
      {
        std::string _kind;
        std::string _name;
        Edition     _edition;
        Arch        _arch;
      };

    private:
      typedef std::list<Entry> StorageT;

    public:
      typedef StorageT::value_type     value_type;
      typedef StorageT::size_type      size_type;
      typedef StorageT::iterator       iterator;
      typedef StorageT::const_iterator const_iterator;

    public:
      Reader()
      {}

      Reader( std::istream & input_r );

    public:
      /** \name Identification.
       * User provided optional data to identify the collection.
      */
      //@{
      /** Get name. */
      const std::string & name() const
      { return _name; }

      /** Get edition. */
      const Edition & edition() const
      { return _edition; }

      /** Get description. */
      const std::string & description() const
      { return _description; }

      /** Get creation date. */
      const Date & ctime() const
      { return _created; }

    public:
      /** \name Collected data. */
      //@{
      /** Whether no data collected so far. */
      bool empty() const
      { return _content.empty(); }

      /** Number of items collected. */
      size_type size() const
      { return _content.size(); }

      /** Iterator to the begin of collected data. */
      const_iterator begin() const
      { return _content.begin(); }

      /** Iterator to the end of collected data. */
      const_iterator end() const
      { return _content.end(); }
      //@}

    private:
      std::string _name;
      Edition     _edition;
      std::string _description;
      Date        _created;

      std::list<Entry> _content;
    };

    /** \relates Reader Stream output */
    inline std::ostream & operator<<( std::ostream & str, const Reader & obj )
    {
      return str << "syscontent(" << obj.name() << "-" << obj.edition()
                 << ", " << obj.size() << " entries, "
                 << " created " << obj.ctime() << ")";
    }

    Reader::Reader( std::istream & input_r )
    {
      xml::Reader reader( input_r );
      for ( ; ! reader.atEnd(); reader.nextNodeOrAttribute() )
        {
          dumpNode( reader );
        }
    }

    /////////////////////////////////////////////////////////////////
  } // namespace syscontent
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////

/******************************************************************
**
**      FUNCTION NAME : main
**      FUNCTION TYPE : int
*/
int main( int argc, char * argv[] )
{
  INT << "===[START]==========================================" << endl;

  if ( 0 )
    {
      zypp::base::LogControl::TmpLineWriter shutUp;
      getZYpp()->initTarget( sysRoot );
      ZYpp::LocaleSet lset;
      lset.insert( Locale("de") );
      getZYpp()->setRequestedLocales( lset );
    }

  ResPool pool( getZYpp()->pool() );
  USR << pool << endl;

  if ( 0 )
    {
      if ( 0 )
        {
          typeof(pool.byKindBegin<Language>()) it = pool.byKindBegin<Language>();
          it->status().setTransact( true, ResStatus::USER );
          ++it;
          it->status().setTransact( true, ResStatus::SOLVER );
          pool.byNameBegin( "SUSE-Linux-SLES-x86_64" )->status().setTransact( true, ResStatus::USER );
        }

      syscontent::Writer contentW;
      contentW.name( "mycollection" )
      .edition( Edition( "1.0" ) )
      .description( "All the cool stuff..." );
      for_each( pool.begin(), pool.end(),
                bind( &syscontent::Writer::addIf, ref(contentW), _1 ) );

      ofstream s( "mycollection.xml" );
      s << contentW;
    }

  if ( 1 )
    {
      syscontent::Reader contentR;
      try
        {
          std::ifstream input( "mycollection.xml" );
          DBG << input << endl;
          contentR = syscontent::Reader( input );
        }
      catch( const Exception & excpt_r )
        {
          ERR << excpt_r << endl;
        }

      MIL << contentR << endl;
    }

  INT << "===[END]============================================" << endl << endl;
  zypp::base::LogControl::instance().logNothing();
  return 0;
}

