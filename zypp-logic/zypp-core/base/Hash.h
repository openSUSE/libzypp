/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/base/Hash.h
 *
*/
#ifndef ZYPP_BASE_HASH_H
#define ZYPP_BASE_HASH_H

#include <iosfwd>
#include <unordered_set>
#include <unordered_map>

/** Define hash function for id based classes.
 * Class has to provide a method \c id() retuning a unique number.
 * \code
 *  // in global namespace define:
 *  ZYPP_DEFINE_ID_HASHABLE( ::zypp::sat::Solvable )
 * \endcode
 * In a C++20 named module interface unit the primary template std::hash is
 * already declared in the global module fragment (via <unordered_set>).
 * Re-declaring it inside the module purview attaches it to the named module
 * and causes a conflicting-declaration error.  We therefore suppress the
 * redundant primary-template forward decl when compiling under modules.
 *
 * GCC defines __cpp_modules when building a named module.
 * Clang 19 does NOT define __cpp_modules even in module interface units
 * (longstanding Clang policy); we therefore also suppress the forward decl
 * unconditionally for Clang — safe because Hash.h itself #includes
 * <unordered_set>, so std::hash is always already declared before the macro
 * expands.
 */
#if defined(__cpp_modules) || defined(__clang__)
#define ZYPP_DEFINE_ID_HASHABLE(C)		\
namespace std {					\
  template<> struct hash<C>			\
  {						\
    size_t operator()( const C & __s ) const	\
    { return __s.id(); }			\
  };						\
}
#else
#define ZYPP_DEFINE_ID_HASHABLE(C)		\
namespace std {					\
  template<class Tp> struct hash;		\
  template<> struct hash<C>			\
  {						\
    size_t operator()( const C & __s ) const	\
    { return __s.id(); }			\
  };						\
}
#endif

///////////////////////////////////////////////////////////////////
namespace std
{
  /** clone function for RW_pointer */
  template<class D>
  inline unordered_set<D> * rwcowClone( const std::unordered_set<D> * rhs )
  { return new std::unordered_set<D>( *rhs ); }

  /** clone function for RW_pointer */
  template<class K, class V>
  inline std::unordered_map<K,V> * rwcowClone( const std::unordered_map<K,V> * rhs )
  { return new std::unordered_map<K,V>( *rhs ); }
} // namespace std
///////////////////////////////////////////////////////////////////
#endif // ZYPP_BASE_HASH_H
