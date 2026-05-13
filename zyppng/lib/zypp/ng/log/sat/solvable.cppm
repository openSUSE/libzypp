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
#include <zypp-core/ng/base/zyppglobal.h>

export module zyppng:log_sat_solvable;

import :log_format;

export namespace zyppng {
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
