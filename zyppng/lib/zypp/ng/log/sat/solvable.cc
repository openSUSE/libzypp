/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
module;
#include <iosfwd>

module zyppng;

import :log_format;
import :log_sat_solvable;  // formatter<Solvable> specialization declaration
import :sat_solvable;

namespace zyppng::log {

  std::ostream &zyppng::log::formatter<sat::Solvable>::stream(std::ostream &str, const sat::Solvable &obj)
  {
    if ( ! obj )
      return str << (obj.isSystem() ? "systemSolvable" : "noSolvable" );

    return str << "(" << obj.id() << ")"
        << ( obj.isKind( ResKind::srcpackage ) ? "srcpackage:" : "" ) << obj.ident()
        << '-' << obj.edition() << '.' << obj.arch()
        //<< "(" << obj.repository().alias() << ")"
    ;
  }

}

