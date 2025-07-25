/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp-core/base/TypeTraits.h
 */
#ifndef ZYPP_TYPETRAITS_H
#define ZYPP_TYPETRAITS_H

#include <type_traits>

///////////////////////////////////////////////////////////////////
// Helper types from https://en.cppreference.com/
namespace std
{
#if __cplusplus < 202002L

#endif // __cplusplus < 202002L


#if __cplusplus < 201703L
  template< class Base, class Derived >
  inline constexpr bool is_base_of_v = is_base_of<Base, Derived>::value;

  template< class T >
  inline constexpr bool is_integral_v = is_integral<T>::value;

  template< class T >
  inline constexpr bool is_pointer_v = is_pointer<T>::value;
#endif // __cplusplus < 201703L


#if __cplusplus < 201402L
  template< bool B, class T, class F >
  using conditional_t = typename conditional<B,T,F>::type;

  template< class T >
  using decay_t = typename decay<T>::type;

  template< bool B, class T = void >
  using enable_if_t = typename enable_if<B,T>::type;

  template< class T >
  using remove_reference_t = typename remove_reference<T>::type;

  template< class T >
  using result_of_t = typename result_of<T>::type;

  template< class T >
  using underlying_type_t = typename underlying_type<T>::type;
#endif // __cplusplus < 201402L
} // namespace std
///////////////////////////////////////////////////////////////////
#endif // ZYPP_TYPETRAITS_H
