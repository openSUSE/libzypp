/*---------------------------------------------------------------------
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

#ifndef ZYPP_NG_LOG_SAT_CAPABILITY_H_INCLUDED
#define ZYPP_NG_LOG_SAT_CAPABILITY_H_INCLUDED

#include <zypp/ng/log/format.h>
#include <iosfwd>
#include <zypp/ng/sat/capability.h>

namespace zyppng {

  namespace sat {
    class Capability;
    class CapDetail;
  }

  namespace log {
    template <>
    struct formatter<sat::Capability> {
        static std::ostream& stream(std::ostream& str, const sat::Capability &obj );
    };

    template <>
    struct formatter<sat::CapDetail> {
        static std::ostream& stream(std::ostream& str, const sat::CapDetail &obj );
    };

    template <>
    struct formatter<sat::CapDetail::Kind> {
        static std::ostream& stream(std::ostream& str, const sat::CapDetail::Kind &obj );
    };

    template <>
    struct formatter<sat::CapDetail::CapRel> {
        static std::ostream& stream(std::ostream& str, const sat::CapDetail::CapRel &obj );
    };
  }

}
#endif
