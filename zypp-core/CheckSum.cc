/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/CheckSum.cc
 *
*/
#include <iostream>
#include <sstream>

#include <zypp/base/Logger.h>
#include <zypp/base/Gettext.h>
#include <zypp/base/String.h>

#include <zypp/CheckSum.h>
#include <zypp/Digest.h>

using std::endl;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  const std::string & CheckSum::md5Type()
  { return Digest::md5(); }

  const std::string & CheckSum::shaType()
  { static std::string _type( "sha" ); return _type; }

  const std::string & CheckSum::sha1Type()
  { return Digest::sha1(); }

  const std::string & CheckSum::sha224Type()
  { return Digest::sha224(); }

  const std::string & CheckSum::sha256Type()
  { return Digest::sha256(); }

  const std::string & CheckSum::sha384Type()
  { return Digest::sha384(); }

  const std::string & CheckSum::sha512Type()
  { return Digest::sha512(); }

  CheckSum::CheckSum( const std::string & type, const std::string & checksum )
  : _type( str::toLower( type ) )
  , _checksum( checksum )
  {
    switch ( checksum.size() )
    {
      case 128:
        if ( _type == sha512Type() )
          return;
        if ( _type.empty() || _type == shaType() )
        {
          _type = sha512Type();
          return;
        }
        // else: dubious
        break;

      case 96:
        if ( _type == sha384Type() )
          return;
        if ( _type.empty() || _type == shaType() )
        {
          _type = sha384Type();
          return;
        }
        // else: dubious
        break;

      case 64:
        if ( _type == sha256Type() )
          return;
        if ( _type.empty() || _type == shaType() )
        {
          _type = sha256Type();
          return;
        }
        // else: dubious
        break;

      case 56:
        if ( _type == sha224Type() )
          return;
        if ( _type.empty() || _type == shaType() )
        {
          _type = sha224Type();
          return;
        }
        // else: dubious
        break;

      case 40:
        if ( _type == sha1Type() )
          return;
        if ( _type.empty() || _type == shaType() )
        {
          _type = sha1Type();
          return;
        }
        // else: dubious
        break;

      case 32:
        if (  _type == md5Type() )
          return;
        if ( _type.empty() )
        {
          _type = md5Type();
          return;
        }
        // else: dubious
        break;

      case 0:
        return; // empty checksum is ok
        break;

      default:
        if ( _type.empty() )
        {
          WAR << "Can't determine type of " << checksum.size() << " byte checksum '" << _checksum << "'" << endl;
          return;
        }
        // else: dubious
        break;
    }

    // dubious: Throw on malformed known types, otherwise log a warning.
    std::string msg = str::form ( _("Dubious type '%s' for %u byte checksum '%s'"),
                                  _type.c_str(), checksum.size(), _checksum.c_str() );
    if (    _type == md5Type()
         || _type == shaType()
         || _type == sha1Type()
         || _type == sha224Type()
         || _type == sha256Type()
         || _type == sha384Type()
         || _type == sha512Type() )
    {
      ZYPP_THROW( CheckSumException( msg ) );
    }
    else
    {
      WAR << msg << endl;
    }
  }

  CheckSum::CheckSum( const std::string & type_r, std::istream & input_r )
  {
    if ( input_r.good() && ! type_r.empty() )
      {
        _type = str::toLower( type_r );
        _checksum = Digest::digest( _type, input_r );
        if ( ! input_r.eof() || _checksum.empty() )
          {
            _type = _checksum = std::string();
          }
      }
  }

  std::string CheckSum::type() const
  { return _type; }

  std::string CheckSum::checksum() const
  { return _checksum; }

  bool CheckSum::empty() const
  { return (checksum().empty() || type().empty()); }

  std::string CheckSum::asString() const
  {
    std::ostringstream str;
    str << *this;
    return str.str();
  }

  std::ostream & operator<<( std::ostream & str, const CheckSum & obj )
  {
    if ( obj.checksum().empty() )
      {
        return str << std::string("NoCheckSum");
      }

    return str << ( obj.type().empty() ? std::string("UNKNOWN") : obj.type() ) << '-' << obj.checksum();
  }

  std::ostream & dumpAsXmlOn( std::ostream & str, const CheckSum & obj )
  {
    const std::string & type( obj.type() );
    const std::string & checksum( obj.checksum() );
    str << "<checksum";
    if ( ! type.empty() ) str << " type=\"" << type << "\"";
    if ( checksum.empty() )
      str << "/>";
    else
      str << ">" << checksum << "</checksum>";
    return str;
  }

   /** \relates CheckSum */
  bool operator==( const CheckSum & lhs, const CheckSum & rhs )
  { return lhs.checksum() == rhs.checksum() && lhs.type() == rhs.type(); }

  /** \relates CheckSum */
  bool operator!=( const CheckSum & lhs, const CheckSum & rhs )
  { return ! ( lhs == rhs ); }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
