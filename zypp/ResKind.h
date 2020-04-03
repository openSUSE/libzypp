/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ResKind.h
 *
*/
#ifndef ZYPP_RESKIND_H
#define ZYPP_RESKIND_H

#include <iosfwd>
#include <string>

#include <zypp/APIConfig.h>
#include <zypp/base/String.h>
#include <zypp/IdStringType.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  /// \class ResKind
  /// \brief Resolvable kinds.
  /// A \b lowercased string and used as identification. Comparison
  /// against string values is always case insensitive.
  ///////////////////////////////////////////////////////////////////
  class ResKind : public IdStringType<ResKind>
  {
    public:
      /** \name Some builtin ResKind constants. */
      //@{
      /** Value representing \c nokind (<tt>""</tt>)*/
      static const ResKind nokind;

      static const ResKind package;
      static const ResKind patch;
      static const ResKind pattern;
      static const ResKind product;
      static const ResKind srcpackage;
      static const ResKind application;
      //@}

      /** Return the builtin kind if \a str_r explicitly prefixed.
       * \a str_r must start with a builtin kind followed by a \c ':'.
       * If no builtin kind is detected, \ref nokind is returned,
       * which usually indicates a \ref package or \ref srcpackage.
       */
      static ResKind explicitBuiltin( const char * str_r );
      /** \overload */
      static ResKind explicitBuiltin( const std::string & str_r )
      { return explicitBuiltin( str_r.c_str() ); }
      /** \overload */
      static ResKind explicitBuiltin( const IdString & str_r )
      { return explicitBuiltin( str_r.c_str() ); }

    public:
      /** Default ctor: \ref nokind */
      ResKind() {}

      /** Ctor taking kind as string. */
      explicit ResKind( sat::detail::IdType id_r )  : _str( str::toLower(IdString(id_r).c_str()) ) {}
      explicit ResKind( const IdString & idstr_r )  : _str( str::toLower(idstr_r.c_str()) ) {}
      explicit ResKind( const std::string & str_r ) : _str( str::toLower(str_r) ) {}
      explicit ResKind( const char * cstr_r )       : _str( str::toLower(cstr_r) ) {}

    public:
      /** Return libsolv identifier for name.
       * Libsolv combines the objects kind and name in a single
       * identifier \c "pattern:kde_multimedia", \b except for packages
       * and source packes. They are not prefixed by any kind string.
      */
      static std::string satIdent( const ResKind & refers_r, const std::string & name_r );
      /** \overload */
      std::string satIdent( const std::string & name_r ) const
      { return satIdent( *this, name_r ); }

    private:
      static int _doCompare( const char * lhs,  const char * rhs )
      {
        if ( lhs == rhs ) return 0;
        if ( lhs && rhs ) return ::strcasecmp( lhs, rhs );
        return( lhs ? 1 : -1 );
      }

    private:
      friend class IdStringType<ResKind>;
      IdString _str;
  };

  /** \relates ResKind XML output. */
  inline std::ostream & dumpAsXmlOn( std::ostream & str, const ResKind & obj )
  { return str << "<kind>" << obj <<  "</kind>"; }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_RESKIND_H
