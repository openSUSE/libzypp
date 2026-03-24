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

#ifndef ZYPP_NG_LOG_SAT_QUEUE_H_INCLUDED
#define ZYPP_NG_LOG_SAT_QUEUE_H_INCLUDED

#include <zypp/ng/log/format.h>
#include <iosfwd>

namespace zyppng {

  namespace sat {
    class Queue;
  }

  namespace log {
    template <>
    struct formatter<sat::Queue> {
        static std::ostream& stream(std::ostream& str, const sat::Queue &obj );
    };
  }

  namespace sat {
    /**
     * relates: Queue Stream output assuming a Solvable queue.
     */
    std::ostream & dumpOn( std::ostream & str, const Queue & obj );
  }

}
#endif
