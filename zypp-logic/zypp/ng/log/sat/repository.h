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

#ifndef ZYPP_NG_LOG_SAT_REPOSITORY_H_INCLUDED
#define ZYPP_NG_LOG_SAT_REPOSITORY_H_INCLUDED

#include <zypp/ng/log/format.h>
#include <iosfwd>

namespace zyppng {

  namespace sat {
    class Repository;
  }

  namespace log {
    template <>
    struct formatter<sat::Repository> {
        static std::ostream& stream(std::ostream& str, const sat::Repository &obj );
    };
  }

}
#endif
