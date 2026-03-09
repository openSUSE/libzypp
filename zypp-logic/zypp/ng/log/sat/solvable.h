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
#ifndef ZYPP_NG_LOG_SAT_SOLVABLE_H_INCLUDED
#define ZYPP_NG_LOG_SAT_SOLVABLE_H_INCLUDED

#include <zypp/ng/log/format.h>

namespace zyppng {
  ZYPP_FWD_DECL_TYPE_WITH_REFS(Context);

  namespace sat {
    class Solvable;
  }

  namespace log {
    template <>
    struct formatter<sat::Solvable> {
        static std::ostream& stream(std::ostream& str, const sat::Solvable &obj );
    };
  }

}
#endif
