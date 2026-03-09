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
#ifndef ZYPP_NG_LOG_FORMAT_H_INCLUDED
#define ZYPP_NG_LOG_FORMAT_H_INCLUDED

#include <ostream>
#include <sstream>
#include <type_traits>

#include <zypp-core/ng/base/zyppglobal.h>
#include <zypp-core/ng/meta/type_traits.h>


namespace zyppng{

  ZYPP_FWD_DECL_TYPE_WITH_REFS(Context);

  namespace log {

    /**
     * \brief Primary trait for object formatting.
     *
     * To support a type \c T in the log layer, you must provide a specialization of
     * this struct. The specialization is valid if it implements at least one of
     * the two static \c stream signatures.
     *
     * \section contract The Interface Contract
     *
     * \subsection context_aware 1. Context-Aware (For SAT Handles/IDs)
     * \code
     * static std::ostream& stream(std::ostream& os, const ContextRef& ctx, const T& obj);
     * \endcode
     * Use this for objects that require system state (like a \c Pool or Config) to be resolved.
     *
     * \subsection independent 2. Context-Independent (For Self-contained Values)
     * \code
     * static std::ostream& stream(std::ostream& os, const T& obj);
     * \endcode
     * Use this for objects that own their data (like \c Arch or \c Edition).
     *
     * \section example implementation Example
     * \code
     * template <>
     * struct formatter<sat::SolvableId> {
     * static std::ostream& stream(std::ostream& os, const ContextRef& ctx, sat::SolvableId id) {
     * return os << ctx.pool().getName(id);
     * }
     * };
     * \endcode
     *
     * \tparam T   The logic type to be formatted.
     * \note If both signatures are provided, \c log::view(ctx, obj) will prioritize
     * the Context-Aware version.
     */
    template <typename T, typename Enable = void>
    struct formatter;

    namespace detail {

      /** \brief Temporary object for streaming, we prefer direct streaming so we have string allocations only when needed */
      template <typename T>
      struct view_proxy {
          const T& obj;

          operator std::string() const {
              std::ostringstream ss;
              ss << *this;
              return ss.str();
          }

          friend std::ostream& operator<<(std::ostream& os, const view_proxy& v) {
              return formatter<T>::stream(os, v.obj);
          }
      };

      /** \brief Temporary object for streaming, we prefer direct streaming so we have string allocations only when needed */
      template <typename T>
      struct ctx_bound_view_proxy {
          const ContextWeakRef ctx;
          const T& obj;

          operator std::string() const {
              std::ostringstream ss;
              ss << *this;
              return ss.str();
          }

          friend std::ostream& operator<<(std::ostream& os, const ctx_bound_view_proxy& v) {
            auto ctxLock = v.ctx.lock();
            if ( ctxLock )
              return formatter<T>::stream(os, ctxLock, v.obj);
            else {
              return os << "<<Error: Context Required for streaming " << typeid(T).name() << ">>";
            }
          }
      };

      /** \brief Detection trait for StaticFormatter implementation. */
        template <typename T, typename = void>
        struct has_view_proxy : std::false_type {};

        template <typename T>
        struct has_view_proxy<T, std::void_t<decltype(formatter<T>::stream(std::declval<std::ostream&>(), std::declval<const T&>()))>>
            : std::true_type {};
    }

    /** \brief Unified format call: Context-Aware. */
    template <typename T>
    auto view(const ContextRef& ctx, const T& obj) {
      return detail::ctx_bound_view_proxy<T>{ ctx, obj };
    }

    /** \brief Unified format call: Context-Independent. */
    template <typename T >
    auto view(const T& obj) {
      return detail::view_proxy<T>{ obj };
    }
  }

  /*!
   * Catch all operator to enable direct streaming for simple types like e.g. Arch that
   * don't need a context to be printed:
   * MIL << "Solvable architecture: " << solvable->arch() << std::endl;
   */
  template <typename T>
  auto operator<<(std::ostream& os, const T& obj)
      -> std::enable_if_t<log::detail::has_view_proxy<T>::value, std::ostream&>
  {
      return os << log::view<T>(obj);
  }
}

#endif
