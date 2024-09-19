/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp-core/Globals.h
 *  \brief	Provides API related macros.
 */
#ifndef ZYPP_GLOBALS_H
#define ZYPP_GLOBALS_H

#include <memory>
#include <zypp-core/APIConfig.h>        // LIBZYPP_ version defines for the LEGACY macro
#include <zypp-core/base/Easy.h>	// some macros used almost everywhere

/**
 * Legacy code we still support.
 *
 * Deprecated items we can't drop immediately because YAST/PK still
 * refer to them or they break binary compatibility, should be
 * enclosed in `#if LEGACY(#)` where # is either a major number(<=99),
 * a soversion [<=9999] or numversion [<=999999].
 *
 */
#ifndef LIBZYPP_VERSION_MAJOR
#error Missing APIConfig.h include (LIBZYPP_VERSION_MAJOR)
#endif
#ifndef LIBZYPP_SOVERSION
#error Missing APIConfig.h include (LIBZYPP_SOVERSION)
#endif
#ifndef LIBZYPP_VERSION
#error Missing APIConfig.h include (LIBZYPP_VERSION)
#endif
#define LEGACY(CL) ( CL < 100 && LIBZYPP_VERSION_MAJOR <= CL ) || ( CL < 10000 && LIBZYPP_SOVERSION <= CL ) || LIBZYPP_VERSION <= CL

/**
 * Generic helper definitions for shared library support.
 *
 * \see e.g. http://gcc.gnu.org/wiki/Visibility
 * \code
 *   extern "C" ZYPP_API void function(int a);
 *   class ZYPP_API SomeClass
 *   {
 *      int c;
 *      ZYPP_LOCAL void privateMethod();  // Only for use within this DSO
 *   public:
 *      Person(int _c) : c(_c) { }
 *      static void foo(int a);
 *   };
 * \endcode
};*/
#if __GNUC__ >= 4
  #define ZYPP_DECL_EXPORT __attribute__ ((visibility ("default")))
  #define ZYPP_DECL_IMPORT __attribute__ ((visibility ("default")))
  #define ZYPP_DECL_HIDDEN __attribute__ ((visibility ("hidden")))
#else
  #define ZYPP_DECL_EXPORT
  #define ZYPP_DECL_IMPORT
  #define ZYPP_DECL_HIDDEN
#endif

#ifdef ZYPP_DLL	//defined if zypp is compiled as DLL
  #define ZYPP_API	ZYPP_DECL_EXPORT
  #define ZYPP_TESTS	ZYPP_DECL_EXPORT
  #define ZYPP_LOCAL	ZYPP_DECL_HIDDEN
#else
  #define ZYPP_API      ZYPP_DECL_IMPORT
  #define ZYPP_TESTS	ZYPP_DECL_IMPORT
  #define ZYPP_LOCAL
#endif

// A small set of internal symbols offered to the deptestomatic
// tool (package libzypp-testsuite-tools) to load and evaluate
// solver testcases.
#define ZYPP_API_DEPTESTOMATIC  ZYPP_API

/**
 * The ZYPP_DEPRECATED macro can be used to trigger compile-time warnings
 * with gcc >= 3.2 when deprecated functions are used.
 *
 * For non-inline functions, the macro is used at the very end of the
 * function declaration, right before the semicolon, unless it's pure
 * virtual:
 *
 * int deprecatedFunc() const ZYPP_DEPRECATED;
 * virtual int deprecatedPureVirtualFunc() const ZYPP_DEPRECATED = 0;
 *
 * Functions which are implemented inline are handled differently:
 * the ZYPP_DEPRECATED macro is used at the front, right before the
 * return type, but after "static" or "virtual":
 *
 * ZYPP_DEPRECATED void deprecatedFuncA() { .. }
 * virtual ZYPP_DEPRECATED int deprecatedFuncB() { .. }
 * static  ZYPP_DEPRECATED bool deprecatedFuncC() { .. }
 *
 * You can also mark whole structs or classes as deprecated, by inserting
 * the ZYPP_DEPRECATED macro after the struct/class keyword, but before
 * the name of the struct/class:
 *
 * class ZYPP_DEPRECATED DeprecatedClass { };
 * struct ZYPP_DEPRECATED DeprecatedStruct { };
 *
 * However, deprecating a struct/class doesn't create a warning for gcc
 * versions <= 3.3 (haven't tried 3.4 yet).  If you want to deprecate a class,
 * also deprecate all member functions as well (which will cause warnings).
 *
 */
#if __GNUC__ - 0 > 3 || (__GNUC__ - 0 == 3 && __GNUC_MINOR__ - 0 >= 2)
  #ifndef ZYPP_DEPRECATED
  #define ZYPP_DEPRECATED __attribute__ ((deprecated))
  #endif
#else
  #ifndef ZYPP_DEPRECATED
  #define ZYPP_DEPRECATED
  #endif
#endif


#ifdef ZYPP_DLL	//defined if zypp is compiled as DLL
// used to flag API to be deprected inside of libzypp.
#define ZYPP_INTERNAL_DEPRECATE ZYPP_DEPRECATED
// used to mark externally used API as internally deprected
#define ZYPP_LEGACY_API	ZYPP_DECL_EXPORT ZYPP_INTERNAL_DEPRECATE
#else
#define ZYPP_INTERNAL_DEPRECATE
#define ZYPP_LEGACY_API
#endif

/**
 * Macro to disable legacy warnings for code that was otherwise marked as
 * internal deprecated, for examples in files that are defining legacy API.
 */
#if __GNUC__ - 0 > 3 || (__GNUC__ - 0 == 3 && __GNUC_MINOR__ - 0 >= 2)
  #ifndef ZYPP_BEGIN_LEGACY_API
  #define ZYPP_BEGIN_LEGACY_API \
    _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated\"") \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")
  #endif

  #ifndef ZYPP_END_LEGACY_API
  #define ZYPP_END_LEGACY_API \
    _Pragma("GCC diagnostic pop")
  #endif

#else
  #define ZYPP_BEGIN_LEGACY_API
  #define ZYPP_END_LEGACY_API
#endif

namespace zyppng {
  template <typename T>
  using Ref = std::shared_ptr<T>;

  template <typename T>
  using WeakRef = std::weak_ptr<T>;
}

namespace zypp {
  template <typename T>
  using Ref = std::shared_ptr<T>;

  template <typename T>
  using WeakRef = std::weak_ptr<T>;
}

/*!
 * Helper macro to declare Ref types
 */
#define ZYPP_FWD_DECL_REFS(T) \
  using T##Ref = Ref<T>; \
  using T##WeakRef = WeakRef<T>

/*
 * Helper Macro to forward declare types and ref types
 */
#define ZYPP_FWD_DECL_TYPE_WITH_REFS(T) \
  class T; \
  ZYPP_FWD_DECL_REFS(T)

#define ZYPP_FWD_DECL_TEMPL_TYPE_WITH_REFS_ARG1(T, TArg1) \
  template< typename TArg1> \
  class T; \
  template< typename TArg1> \
  using T##Ref = Ref<T<TArg1>>; \
  template< typename TArg1> \
  using T##WeakRef = WeakRef<T<TArg1>>


//@TODO enable for c++20
#if 0
#define ZYPP_FWD_DECL_TEMPL_TYPE_WITH_REFS(T, TArg1, ...) \
  template< typename TArg1 __VA_OPT__(, typename) __VA_ARGS__  > \
  class T; \
  template< typename TArg1 __VA_OPT__(, typename) __VA_ARGS__  > \
  using T##Ref = std::shared_ptr<T<TArg1 __VA_OPT__(,) __VA_ARGS__>>; \
  template< typename TArg1 __VA_OPT__(, typename) __VA_ARGS__  > \
  using T##WeakRef = std::weak_ptr<T<TArg1 __VA_OPT__(,) __VA_ARGS__ >>
#endif


#endif
