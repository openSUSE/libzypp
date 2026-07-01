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

module;
#include <iosfwd>

export module zyppng:log_sat_queue;

import :log_format;
import :sat_queue;  // brings Queue — do NOT forward-declare it here

export namespace zyppng {

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
