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

export module zyppng:log_sat_repository;

import :log_format;
import :sat_repository;  // brings Repository — do NOT forward-declare it here

export namespace zyppng {

  namespace log {
    template <>
    struct formatter<sat::Repository> {
        static std::ostream& stream(std::ostream& str, const sat::Repository &obj );
    };
  }

}
