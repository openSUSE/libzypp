/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
//
// ZYPPNG_TEST_EXPORT — declaration-site marker for module-internal symbols
// that need to be reachable from test executables.
//
// Expands to `export` only in the test-twin build of the module
// (`zyppng-objlib-test`, where ZYPPNG_TEST_BUILD is defined).  In the
// production build the macro expands to nothing, so the symbol keeps
// module linkage and stays invisible to `import zyppng;` consumers.
//
// Usage at the declaration site:
//
//   namespace zyppng::sat::detail {
//     ZYPPNG_TEST_EXPORT inline constexpr SolvableIdType noSolvableId{ ID_NULL };
//     ZYPPNG_TEST_EXPORT class IBasicPoolComponent { ... };
//   }
//
// This header is intended to be #included in the global module fragment
// of any .cppm that needs it.  It contains no other declarations and is
// therefore safe to include unconditionally.

#pragma once

#ifdef ZYPPNG_TEST_BUILD
#  define ZYPPNG_TEST_EXPORT export
#else
#  define ZYPPNG_TEST_EXPORT
#endif
