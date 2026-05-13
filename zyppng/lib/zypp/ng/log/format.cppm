/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
module;
#include <ostream>
#include <sstream>
#include <type_traits>
#include <typeinfo>
#include <zypp-core/ng/base/zyppglobal.h>
#include <zypp-core/ng/meta/type_traits.h>

export module zyppng:log_format;

// Context is defined in zyppng:context — forward-declare here via the
// macro so log_format has no circular dependency on the full context partition.
export namespace zyppng {
  ZYPP_FWD_DECL_TYPE_WITH_REFS( Context );
} // namespace zyppng

export namespace zyppng::log {

  /**
   * \brief Primary trait for object formatting.
   *
   * Specialise this struct for any type T that needs log-layer formatting.
   * At least one of the two static \c stream signatures must be provided:
   *
   *   1. Context-Aware (for SAT handles/IDs):
   *      static std::ostream& stream(std::ostream&, const ContextRef&, const T&);
   *
   *   2. Context-Independent (for self-contained values):
   *      static std::ostream& stream(std::ostream&, const T&);
   *
   * If both are provided, log::view(ctx, obj) prioritises the context-aware form.
   */
  template <typename T, typename Enable = void>
  struct formatter;

} // namespace zyppng::log

// Module-purview-only — internal proxies and detection traits.
namespace zyppng::log::detail {

    /** Temporary proxy for context-independent streaming — avoids string
     *  allocation unless the result is actually converted to std::string. */
    template <typename T>
    struct view_proxy {
      const T & obj;

      operator std::string() const {
        std::ostringstream ss;
        ss << *this;
        return ss.str();
      }

      friend std::ostream & operator<<( std::ostream & os, const view_proxy & v ) {
        return ::zyppng::log::formatter<T>::stream( os, v.obj );
      }
    };

    /** Temporary proxy for context-aware streaming. */
    template <typename T>
    struct ctx_bound_view_proxy {
      const ::zyppng::ContextWeakRef ctx;
      const T & obj;

      operator std::string() const {
        std::ostringstream ss;
        ss << *this;
        return ss.str();
      }

      friend std::ostream & operator<<( std::ostream & os, const ctx_bound_view_proxy & v ) {
        auto ctxLock = v.ctx.lock();
        if ( ctxLock )
          return ::zyppng::log::formatter<T>::stream( os, ctxLock, v.obj );
        else
          return os << "<<Error: Context Required for streaming " << typeid(T).name() << ">>";
      }
    };

    /** Detection trait: true if formatter<T> provides a context-independent stream(). */
    template <typename T, typename = void>
    struct has_view_proxy : std::false_type {};

    template <typename T>
    struct has_view_proxy<T,
      std::void_t<decltype( ::zyppng::log::formatter<T>::stream(
        std::declval<std::ostream &>(), std::declval<const T &>() ) )>>
      : std::true_type {};

} // namespace zyppng::log::detail

export namespace zyppng::log {
  /** Unified format call: Context-Aware. */
  template <typename T>
  auto view( const ContextRef & ctx, const T & obj ) {
    return detail::ctx_bound_view_proxy<T>{ ctx, obj };
  }

  /** Unified format call: Context-Independent. */
  template <typename T>
  auto view( const T & obj ) {
    return detail::view_proxy<T>{ obj };
  }

} // namespace zyppng::log

export namespace zyppng {

  /**
   * Catch-all operator<< for types that have a context-independent formatter<T>
   * specialisation.  Enables: MIL << "arch: " << solvable->arch() << std::endl;
   */
  template <typename T>
  auto operator<<( std::ostream & os, const T & obj )
    -> std::enable_if_t<log::detail::has_view_proxy<T>::value, std::ostream &>
  {
    return os << log::view<T>( obj );
  }

} // namespace zyppng
