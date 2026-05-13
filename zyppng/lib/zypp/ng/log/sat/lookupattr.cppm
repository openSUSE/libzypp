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



export module zyppng:log_sat_lookupattr;

import :log_format;
import :sat_lookupattr;
import :sat_lookupattrtools;

export namespace zyppng {

  namespace log {
    template <>
    struct formatter<sat::LookupAttr> {
        static std::ostream& stream(std::ostream& str, const sat::LookupAttr &obj );
    };

    template <>
    struct formatter<sat::detail::DIWrap> {
        static std::ostream& stream(std::ostream& str, const sat::detail::DIWrap &obj );
    };

    template <>
    struct formatter<sat::LookupAttr::iterator> {
        static std::ostream& stream(std::ostream& str, const sat::LookupAttr::iterator &obj );
    };

    template <>
    struct formatter<sat::detail::CDataiterator *> {
        static std::ostream& stream(std::ostream& str, const sat::detail::CDataiterator *obj );
    };

    /**
     * \brief Formatter for \ref zyppng::sat::ArrayAttr<TResult,TAttr>.
     *
     * Iterates the range and streams each element via its own formatter,
     * producing output of the form:
     * \code
     *   ArrayAttr[elem0 elem1 elem2]
     * \endcode
     * Each element is streamed using \c log::view(str, elem), so any
     * type-specific formatter registered for \c TResult is honoured.
     */
    template <class TResult, class TAttr>
    struct formatter<sat::ArrayAttr<TResult, TAttr>> {
        static std::ostream& stream( std::ostream& str,
                                     const sat::ArrayAttr<TResult, TAttr> & obj )
        {
            str << "ArrayAttr[";
            bool first = true;
            for ( const TResult & val : obj ) {
                if ( !first ) str << ' ';
                // Route through the log layer so TResult-specific formatters fire.
                str << log::view( val );
                first = false;
            }
            return str << ']';
        }
    };
  }

}
