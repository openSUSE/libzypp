/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "solvable.h"

#include <zypp/ng/sat/solvable.h>

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

