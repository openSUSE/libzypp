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

export module zyppng:log_sat_capabilities;

import :log_format;
import :sat_capabilities;  // brings Capabilities — do NOT forward-declare it here

export namespace zyppng {

  namespace log {
    template <>
    struct formatter<sat::Capabilities> {
        static std::ostream& stream( std::ostream& str, const sat::Capabilities &obj );
    };
  }

}
