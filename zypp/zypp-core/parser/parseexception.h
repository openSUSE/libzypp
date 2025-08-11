/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/parser/tagfile/ParseException.h
 *
*/
#ifndef ZYPP_CORE_PARSER_PARSEEXCEPTION_H
#define ZYPP_CORE_PARSER_PARSEEXCEPTION_H

#include <iosfwd>
#include <string>

#include <zypp-core/base/Exception.h>
#include <zypp-core/base/UserRequestException>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace parser
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : ParseException
    //
    /** */
    class ZYPP_API ParseException : public Exception
    {
    public:
      /** Default ctor */
      ParseException();
      /** Ctor */
      ParseException( const std::string & msg_r );
        /** Dtor */
      ~ParseException() throw() override;
    protected:
      std::ostream & dumpOn( std::ostream & str ) const override;
    };
    ///////////////////////////////////////////////////////////////////

    /////////////////////////////////////////////////////////////////
  } // namespace parser
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_CORE_PARSER_PARSEEXCEPTION_H
