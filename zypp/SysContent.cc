/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/SysContent.cc
 *
*/
#include <iostream>
#include "zypp/base/Logger.h"

#include "zypp/SysContent.h"

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace syscontent
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    namespace
    { /////////////////////////////////////////////////////////////////

      inline std::string attrIf( const std::string & tag_r,
                                 const std::string & value_r )
      {
        std::string ret;
        if ( ! value_r.empty() )
          {
            ret += " ";
            ret += tag_r;
            ret += "=\"";
            ret += value_r;
            ret += "\"";
          }
        return ret;
      }

      /////////////////////////////////////////////////////////////////
    } // namespace
    ///////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	METHOD NAME : Writer::writeXml
    //	METHOD TYPE : std::ostream &
    //
    std::ostream & Writer::writeXml( std::ostream & str ) const
    {
      // intro
      str << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
      str << "<syscontent>\n";
      // ident data
      str << "  <ident>\n";
      str << "    <name>" << _name << "</name>\n";
      str << "    <version"
          << attrIf( "epoch", str::numstring(_edition.epoch()) )
          << attrIf( "ver",   _edition.version() )
          << attrIf( "rel",   _edition.release() )
          << "/>\n";
      str << "    <description>" << _description << "</description>\n";
      str << "    <created>" << Date::now().asSeconds() << "</created>\n";
      str << "  </ident>\n";
      // ResObjects
      str << "  <onsys>\n";
      for ( iterator it = begin(); it != end(); ++it )
        {
          str << "    <entry"
              << attrIf( "kind",  (*it)->kind().asString() )
              << attrIf( "name",  (*it)->name() )
              << attrIf( "epoch", str::numstring((*it)->edition().epoch()) )
              << attrIf( "ver",   (*it)->edition().version() )
              << attrIf( "rel",   (*it)->edition().release() )
              << attrIf( "arch",  (*it)->arch().asString() )
              << "/>\n";
        }
      str << "  </onsys>\n";
      // extro
      str << "</syscontent>" << endl;
      return str;
    };

    /////////////////////////////////////////////////////////////////
  } // namespace syscontent
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
