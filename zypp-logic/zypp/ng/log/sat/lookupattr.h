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

#ifndef ZYPP_NG_LOG_SAT_LOOKUPATTR_H_INCLUDED
#define ZYPP_NG_LOG_SAT_LOOKUPATTR_H_INCLUDED

#include <zypp/ng/log/format.h>
#include <zypp/ng/sat/lookupattr.h>

namespace zyppng {

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
  }

}
#endif
