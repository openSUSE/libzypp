/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
module;
#include "zypp-core/base/LogTools.h"

module zyppng;

import :log_sat_capability;
import :log_sat_capabilities;  // formatter<Capabilities> specialization declaration
import :sat_capabilities;

namespace zyppng::log {

  std::ostream &formatter<sat::Capabilities>::stream(std::ostream &str, const sat::Capabilities &obj)
  {
    return str;
    //return zypp::dumpRangeLine( str << "Capabilities ", obj.begin(), obj.end() );
  }

}
