/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\----------------------------------------------------------------------/
*
* Forward declarations for zypp-core pointer/reference types.
*
* Include this in a module GMF (#include inside the module; block) whenever
* a cppm purview needs only Ref/WeakRef/Ptr aliases or forward-declared types
* — not their full definitions.
*
* Rules:
*   - Only types used purely by pointer, reference, or as opaque return/param
*     types in declarations belong here.
*   - Types used as base classes, by-value members, or in template
*     instantiations that require the template body require their full header.
*   - Types used by macros that expand inline (e.g. ZYPP_DEFINE_ID_HASHABLE)
*     require their full header.
*   - Add new entries as cppm files require them — do not pre-populate
*     speculatively.
*
* Mirror of zypp-media/ng/providefwd.h for the zypp-core layer.
*/
#ifndef ZYPP_CORE_NG_FWD_H_INCLUDED
#define ZYPP_CORE_NG_FWD_H_INCLUDED

// Provides ZYPP_FWD_DECL_TYPE_WITH_REFS, Ref<T>, WeakRef<T>
#include <zypp-core/ng/base/zyppglobal.h>

// ---------------------------------------------------------------------------
// zyppng:: types — used by Ref/WeakRef (std::shared_ptr / std::weak_ptr)
// ---------------------------------------------------------------------------
namespace zyppng {

  // Add further zyppng types below as cppm files require them.
  ZYPP_FWD_DECL_TYPE_WITH_REFS( EventDispatcher );

} // namespace zyppng

// ---------------------------------------------------------------------------
// zypp:: types — forward declarations sufficient for cppm interface units.
//
// Rule: only add types here if they are used EXCLUSIVELY as const-ref / ref
// parameters or plain type aliases in cppm purviews.  Types returned by value
// or used in inline function bodies require their full header in the cppm GMF
// — forward-declaring them triggers a GCC 16 ICE (is_really_empty_class) when
// incomplete return types propagate through the module graph.
//
// Types explicitly NOT listed here for this reason:
//   zypp::ByteCount  — returned by value from Solvable
//   zypp::CheckSum   — returned by value from Solvable
//   zypp::Date       — returned by value from Solvable and Repository
//   zypp::Pathname   — returned by value and used in inline bodies in Pool
//
// Note on zypp:: legacy pointer types: zypp uses boost::intrusive_ptr (via
// zypp::intrusive_ptr) — there is no std:: equivalent.  Types needing _Ptr
// typedefs require PtrTypes.h and DEFINE_PTR_TYPE; add them only when the
// #if 0 blocks gating legacy integration are lifted.
// ---------------------------------------------------------------------------
namespace zypp {

  // Used as const-ref parameter in declarations only.
  class InputStream;

  // Used as type aliases and const-ref in lookupattr / strmatcher interfaces.
  class Match;
  class StrMatcher;
  struct MatchException;

  // Add further zypp:: types below as cppm files require them.

} // namespace zypp

#endif // ZYPP_CORE_NG_FWD_H_INCLUDED
