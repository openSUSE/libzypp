/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/base/Flags.h
 *
*/
#ifndef ZYPP_BASE_FLAGS_H
#define ZYPP_BASE_FLAGS_H

#include "zypp/base/Macros.h"
#include "zypp/base/String.h"

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////
  namespace base
  { /////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////////////
    //
    //	CLASS NAME : Flags<Enum>
    //
    /** A type-safe way of storing OR-combinations of enum values (like QTs QFlags).
     * \see <a href="http://doc.trolltech.com/4.1/qflags.html">QFlags Class Reference</a>
     * \code
     *  class RpmDb
     *  {
     *    public:
     *      enum DbStateInfoBits {
     *        DbSI_NO_INIT	= 0x0000,
     *        DbSI_HAVE_V4	= 0x0001,
     *        DbSI_MADE_V4	= 0x0002,
     *        DbSI_MODIFIED_V4	= 0x0004,
     *        DbSI_HAVE_V3	= 0x0008,
     *        DbSI_HAVE_V3TOV4	= 0x0010,
     *        DbSI_MADE_V3TOV4	= 0x0020
     *      };
     *
     *      ZYPP_DECLARE_FLAGS(DbStateInfo,DbStateInfoBits);
     *  };
     *  ZYPP_DECLARE_OPERATORS_FOR_FLAGS(RpmDb::DbStateInfo);
     *
     *  ...
     *  enum Other { OTHERVAL = 13 };
     *  {
     *    XRpmDb::DbStateInfo s;
     *    s = XRpmDb::DbSI_MODIFIED_V4|XRpmDb::DbSI_HAVE_V4;
     *    // s |= OTHERVAL; // As desired: it does not compile
     *  }
     * \endcode
     */
    template<typename Enum>
    class Flags
    {
      public:
        typedef Enum enum_type;

      public:
        Flags()                               : _val( 0 ) {}
        Flags( Enum flag_r )                  : _val( flag_r ) {}
        explicit Flags( unsigned flag_r )     : _val( flag_r ) {}

        Flags & operator&=( Flags rhs )       { _val &= rhs._val; return *this; }
        Flags & operator&=( Enum rhs )        { _val &= rhs;      return *this; }

        Flags & operator|=( Flags rhs )       { _val |= rhs._val; return *this; }
        Flags & operator|=( Enum rhs )        { _val |= rhs;      return *this; }

        Flags & operator^=( Flags rhs )       { _val ^= rhs._val; return *this; }
        Flags & operator^=( Enum rhs )        { _val ^= rhs;      return *this; }

      public:
        operator unsigned() const             { return _val; }

        Flags operator&( Flags rhs ) const    { return Flags( *this ) &= rhs; }
        Flags operator&( Enum rhs ) const     { return Flags( *this ) &= rhs; }

        Flags operator|( Flags rhs ) const    { return Flags( *this ) |= rhs; }
        Flags operator|( Enum rhs ) const     { return Flags( *this ) |= rhs; }

        Flags operator^( Flags rhs ) const    { return Flags( *this ) ^= rhs; }
        Flags operator^( Enum rhs ) const     { return Flags( *this ) ^= rhs; }

        Flags operator~() const               { return Flags( ~_val ); }

      public:
        Flags & setFlag( Flags flag_r, bool newval_r ) { return( newval_r ? setFlag(flag_r) : unsetFlag(flag_r) ); }
        Flags & setFlag( Enum flag_r, bool newval_r )  { return( newval_r ? setFlag(flag_r) : unsetFlag(flag_r) ); }

        Flags & setFlag( Flags flag_r )       { _val |= flag_r; return *this; }
        Flags & setFlag( Enum flag_r )        { _val |= flag_r; return *this; }

        Flags & unsetFlag( Flags flag_r )     { _val &= ~flag_r; return *this; }
        Flags & unsetFlag( Enum flag_r )      { _val &= ~flag_r; return *this; }

        bool testFlag( Flags flag_r ) const   { return ( _val & flag_r ) == flag_r; }
        bool testFlag( Enum flag_r ) const    { return ( _val & flag_r ) == flag_r; }

      private:
        unsigned _val;
    };
    ///////////////////////////////////////////////////////////////////

    template<typename Enum>
    inline std::ostream & operator<<( std::ostream & str, const Flags<Enum> & obj )
    { return str << str::hexstring(obj); }

    /** \relates Flags */
#define ZYPP_DECLARE_FLAGS(Name,Enum) typedef zypp::base::Flags<Enum> Name

    /** \relates Flags */
#define ZYPP_DECLARE_OPERATORS_FOR_FLAGS(Name) \
inline Name operator&( Name::enum_type lhs, Name::enum_type rhs )	{ return Name( lhs ) &= rhs; }	\
inline Name operator&( Name::enum_type lhs, Name rhs )			{ return rhs &= lhs; }		\
inline Name operator|( Name::enum_type lhs, Name::enum_type rhs )	{ return Name( lhs ) |= rhs; }	\
inline Name operator|( Name::enum_type lhs, Name rhs )			{ return rhs |= lhs; }		\
inline Name operator^( Name::enum_type lhs, Name::enum_type rhs )	{ return Name( lhs ) ^= rhs; }	\
inline Name operator^( Name::enum_type lhs, Name rhs )			{ return rhs ^= lhs; }		\
inline Name operator~( Name::enum_type lhs )				{ return ~Name( lhs ); }

    /** \relates Flags */
#define ZYPP_DECLARE_FLAGS_AND_OPERATORS(Name,Enum) \
    ZYPP_DECLARE_FLAGS(Name,Enum); \
    ZYPP_DECLARE_OPERATORS_FOR_FLAGS(Name)

    /////////////////////////////////////////////////////////////////
  } // namespace base
  ///////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_BASE_FLAGS_H
