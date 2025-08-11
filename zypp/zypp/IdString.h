/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/sat/IdString.h
 *
*/
#ifndef ZYPP_SAT_IDSTR_H
#define ZYPP_SAT_IDSTR_H

#include <iosfwd>
#include <string>
#include <string_view>

#include <boost/utility/string_ref_fwd.hpp>

#include <zypp-core/Globals.h>
#include <zypp/sat/detail/PoolMember.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  class IdString;
  using IdStringSet = std::unordered_set<IdString>;

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : IdString
  //
  /** Access to the sat-pools string space.
   *
   * Construction from string will place a copy of the string in the
   * string space, if it is not already present.
   *
   * While comparison differs between \ref IdString::Null and \ref IdString::Empty
   * ( \c NULL and \c "" ), both are represented by an empty string \c "".
   */
  class ZYPP_API IdString : protected sat::detail::PoolMember
  {
    public:
      using IdType = sat::detail::IdType;

    public:
      /** Default ctor, empty string. */
      constexpr IdString() : _id( sat::detail::emptyId ) {}

      /** Ctor from id. */
      constexpr explicit IdString( IdType id_r ) : _id( id_r ) {}

      /** Ctor from string. */
      explicit IdString( const char * str_r );

      /** Ctor from string (pointer,length). */
      IdString( const char * str_r, unsigned len_r );

      /** Ctor from string. */
      explicit IdString( const std::string & str_r );

      /** Ctor from boost::string_ref. */
      explicit IdString( boost::string_ref str_r );

#ifdef __cpp_lib_string_view
      explicit IdString( std::string_view str_r )
      : IdString( str_r.data(), str_r.size() )
      {}
#endif

    public:
      /** No or Null string ( Id \c 0 ). */
      static const IdString Null;

      /** Empty string. */
      static const IdString Empty;

    public:
      /** Evaluate in a boolean context <tt>( != \c Null )</tt>. */
      constexpr explicit operator bool() const
      { return _id; }

      /** Whether the string is empty.
       * This is true for \ref Null and \ref Empty.
       */
      constexpr bool empty() const
      { return( _id == sat::detail::emptyId || _id == sat::detail::noId ); }

      /** The strings size. */
      unsigned size() const;

    public:
      /** Conversion to <tt>const char *</tt> */
      const char * c_str() const;

      /** Conversion to <tt>std::string</tt> */
      std::string asString() const
      { return c_str(); }

      /** Explicit conversion to std::string */
      explicit operator std::string() const
      { return asString(); }

#ifdef __cpp_lib_string_view
      /** As \c std::string_view */
      std::string_view asStringView() const
      { return { c_str(), size() }; }

      /** Explicit conversion to \c std::string_view */
      explicit operator std::string_view() const
      { return asStringView(); }
#endif

    public:
      /** Fast compare equal. */
      bool compareEQ( const IdString & rhs ) const
      { return( _id == rhs.id() ); }

      /** Compare IdString returning <tt>-1,0,1</tt>. */
      int compare( const IdString & rhs ) const;

      /** \overload */
      int compare( const char * rhs ) const;

      /** \overload */
      int compare( const std::string & rhs ) const
      { return compare( rhs.c_str() ); }

    public:
      /** Expert backdoor. */
      IdType id() const
      { return _id; }

    private:
      IdType _id;
  };
  ///////////////////////////////////////////////////////////////////

  /** \relates IdString Stream output */
  std::ostream & operator<<( std::ostream & str, const IdString & obj ) ZYPP_API;

  /** \relates IdString Stream output */
  std::ostream & dumpOn( std::ostream & str, const IdString & obj ) ZYPP_API;

  /** \relates IdString Equal */
  inline bool operator==( const IdString & lhs, const IdString & rhs )
  { return lhs.compareEQ( rhs ); }
  /** \overload */
  inline bool operator==( const IdString & lhs, const char * rhs )
  { return lhs.compare( rhs ) == 0; }
  /** \overload */
  inline bool operator==( const IdString & lhs, const std::string & rhs )
  { return lhs.compare( rhs ) == 0; }
  /** \overload */
  inline bool operator==( const char * lhs, const IdString & rhs )
  { return rhs.compare( lhs ) == 0; }
  /** \overload */
  inline bool operator==( const std::string & lhs, const IdString & rhs )
  { return rhs.compare( lhs ) == 0; }

  /** \relates IdString NotEqual */
  inline bool operator!=( const IdString & lhs, const IdString & rhs )
  { return ! lhs.compareEQ( rhs ); }
  /** \overload */
  inline bool operator!=( const IdString & lhs, const char * rhs )
  { return lhs.compare( rhs ) != 0; }
  /** \overload */
  inline bool operator!=( const IdString & lhs, const std::string & rhs )
  { return lhs.compare( rhs ) != 0; }
  /** \overload */
  inline bool operator!=( const char * lhs, const IdString & rhs )
  { return rhs.compare( lhs ) != 0; }
  /** \overload */
  inline bool operator!=( const std::string & lhs, const IdString & rhs )
  { return rhs.compare( lhs ) != 0; }

  /** \relates IdString Less */
  inline bool operator<( const IdString & lhs, const IdString & rhs )
  { return lhs.compare( rhs ) < 0; }
  /** \overload */
  inline bool operator<( const IdString & lhs, const char * rhs )
  { return lhs.compare( rhs ) < 0; }
  /** \overload */
  inline bool operator<( const IdString & lhs, const std::string & rhs )
  { return lhs.compare( rhs ) < 0; }
  /** \overload */
  inline bool operator<( const char * lhs, const IdString & rhs )
  { return rhs.compare( lhs ) >= 0; }
  /** \overload */
  inline bool operator<( const std::string & lhs, const IdString & rhs )
  { return rhs.compare( lhs ) >= 0; }

  /** \relates IdString LessEqual*/
  inline bool operator<=( const IdString & lhs, const IdString & rhs )
  { return lhs.compare( rhs ) <= 0; }
  /** \overload */
  inline bool operator<=( const IdString & lhs, const char * rhs )
  { return lhs.compare( rhs ) <= 0; }
  /** \overload */
  inline bool operator<=( const IdString & lhs, const std::string & rhs )
  { return lhs.compare( rhs ) <= 0; }
  /** \overload */
  inline bool operator<=( const char * lhs, const IdString & rhs )
  { return rhs.compare( lhs ) > 0; }
  /** \overload */
  inline bool operator<=( const std::string & lhs, const IdString & rhs )
  { return rhs.compare( lhs ) > 0; }

  /** \relates IdString Greater */
  inline bool operator>( const IdString & lhs, const IdString & rhs )
  { return lhs.compare( rhs ) > 0; }
  /** \overload */
  inline bool operator>( const IdString & lhs, const char * rhs )
  { return lhs.compare( rhs ) > 0; }
  /** \overload */
  inline bool operator>( const IdString & lhs, const std::string & rhs )
  { return lhs.compare( rhs ) > 0; }
  /** \overload */
  inline bool operator>( const char * lhs, const IdString & rhs )
  { return rhs.compare( lhs ) <= 0; }
  /** \overload */
  inline bool operator>( const std::string & lhs, const IdString & rhs )
  { return rhs.compare( lhs ) <= 0; }

  /** \relates IdString GreaterEqual */
  inline bool operator>=( const IdString & lhs, const IdString & rhs )
  { return lhs.compare( rhs ) >= 0; }
  /** \overload */
  inline bool operator>=( const IdString & lhs, const char * rhs )
  { return lhs.compare( rhs ) >= 0; }
  /** \overload */
  inline bool operator>=( const IdString & lhs, const std::string & rhs )
  { return lhs.compare( rhs ) >= 0; }
  /** \overload */
  inline bool operator>=( const char * lhs, const IdString & rhs )
  { return rhs.compare( lhs ) < 0; }
  /** \overload */
  inline bool operator>=( const std::string & lhs, const IdString & rhs )
  { return rhs.compare( lhs ) < 0; }

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////

ZYPP_DEFINE_ID_HASHABLE( ::zypp::IdString );

#endif // ZYPP_SAT_IDSTR_H
