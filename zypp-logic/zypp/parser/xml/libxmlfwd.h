/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/parser/xml/libxmlfwd.h
 *
*/
#ifndef ZYPP_PARSER_XML_LIBXMLFWD_H
#define ZYPP_PARSER_XML_LIBXMLFWD_H

#include <libxml/xmlreader.h>
#include <libxml/xmlerror.h>

#include <iosfwd>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace xml
  { /////////////////////////////////////////////////////////////////

    using ReadState = xmlTextReaderMode;
    /** \relates ReadState Stream output. */
    std::ostream & operator<<( std::ostream & str, const ReadState & obj );

    using NodeType = xmlReaderTypes;
    /** \relates NodeType Stream output. */
    std::ostream & operator<<( std::ostream & str, const NodeType & obj );

    /////////////////////////////////////////////////////////////////
  } // namespace xml
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_PARSER_XML_LIBXMLFWD_H
