/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "repository.h"
#include <zypp/ng/sat/repository.h>
#include <iostream>

namespace zyppng {

  namespace log {
    std::ostream &formatter<sat::Repository>::stream(std::ostream &str, const sat::Repository &obj)
    {
      if ( ! obj )
        return str << "noRepository";

      return str << "sat::repo(" << obj.alias() << ")"
                 << "{"
                 << "prio " << obj.satInternalPriority() << '.' << obj.satInternalSubPriority()
                 << ", size " << obj.solvablesSize()
                 << "}";
    }
  }

} // namespace zyppng
