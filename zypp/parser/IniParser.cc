/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/parser/IniParser.cc
 *
*/
#include <iostream>
#include <sstream>

#include "zypp/base/Logger.h"
#include "zypp/base/String.h"
#include "zypp/base/IOStream.h"

#include "zypp/parser/IniParser.h"

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////
namespace parser
{ /////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
//
//	METHOD NAME : IniParser::IniParser
//	METHOD TYPE : Ctor
//
IniParser::IniParser()
  : _line_nr(0)
{}

///////////////////////////////////////////////////////////////////
//
//	METHOD NAME : IniParser::~IniParser
//	METHOD TYPE : Dtor
//
IniParser::~IniParser()
{}

void IniParser::beginParse()
{}

void IniParser::consume( const std::string &section, const std::string &key, const std::string &value )
{}

void IniParser::consume( const std::string &section )
{}

void IniParser::endParse()
{}

///////////////////////////////////////////////////////////////////
//
//	METHOD NAME : IniParser::parse
//	METHOD TYPE : void
//
void IniParser::parse( const InputStream & input_r )
{
  MIL << "Start parsing " << input_r << endl;
  _inputname = input_r.name();
  beginParse();

  iostr::EachLine line( input_r );
  for ( ; line; line.next() )
  {
    std::string trimmed = str::trim(*line);

    if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#')
      continue ; /* Comment lines */

    if (trimmed[0] == '[')
    {
      std::string section = trimmed.substr(1, trimmed.find(']')-1);
      consume(section);
      section.swap(_current_section);
      continue;
    }

    std::string::size_type pos = trimmed.find('=');

    if (pos != std::string::npos)
    {
      std::string key = str::rtrim(trimmed.substr(0, pos));
      std::string value = str::ltrim(trimmed.substr(pos+1));
      consume( _current_section, key, value);
    }

  }

  endParse();
  _inputname.clear();
  MIL << "Done parsing " << input_r << endl;
}

/////////////////////////////////////////////////////////////////
} // namespace parser
///////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
