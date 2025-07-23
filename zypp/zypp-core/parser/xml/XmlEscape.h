/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/parser/xml/XmlEscape.h
 *
*/
#ifndef ZYPP_PARSER_XML_XMLESCAPE_H
#define ZYPP_PARSER_XML_XMLESCAPE_H

#include <sstream>
#include <string>
#include <zypp-core/Globals.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{
  ///////////////////////////////////////////////////////////////////
  namespace xml
  {
    ///////////////////////////////////////////////////////////////////
    namespace detail
    {
      struct ZYPP_API EscapedString
      {
        EscapedString( const std::string & in_r ) : _in( in_r ) {}
        std::ostream & dumpOn( std::ostream & str ) const;
        std::string asString() const
        { std::ostringstream str; dumpOn( str ); return str.str(); }
        operator std::string() const
        { return asString(); }
      private:
        const std::string & _in;
      };

      /** \relates EscapedString Stream output */
      inline std::ostream & operator<<( std::ostream & str, const EscapedString & obj )
      { return obj.dumpOn( str ); }

    } // detail
    ///////////////////////////////////////////////////////////////////

    /** Escape xml special charaters (<tt>& -> &amp;</tt>; from IoBind library).
     * The \ref detail::EscapedString can be dumped to an ostream and implicitly
     * converts into a std::string.
     */
    ZYPP_API inline detail::EscapedString escape( const std::string & in_r )
    { return detail::EscapedString( in_r ); }

    /** Unescape xml special charaters (<tt>&amp; -> &</tt>; from IoBind library) */
    std::string unescape( const std::string & in_r ) ZYPP_API;

  } // namespace xml
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_PARSER_XML_XMLESCAPE_H
