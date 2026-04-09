/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "capabilities.h"
#include "capability.h"
#include "zypp-core/base/LogTools.h"
#include "zypp/ng/sat/capabilities.h"

namespace zyppng::log {

  std::ostream &formatter<sat::Capabilities>::stream(std::ostream &str, const sat::Capabilities &obj)
  {
    return str;
    //return zypp::dumpRangeLine( str << "Capabilities ", obj.begin(), obj.end() );
  }

}
