/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "queue.h"

#include <iostream>
#include <zypp-core/base/LogTools.h>
#include <zypp/ng/sat/queue.h>
#include <zypp/ng/sat/solvable.h>

#include <zypp/ng/log/sat/solvable.h>

using std::endl;

namespace zyppng {

  namespace log {
    std::ostream &formatter<sat::Queue>::stream(std::ostream &str, const sat::Queue &obj)
    {
      return zypp::dumpRangeLine( str << "Queue ", obj.begin(), obj.end() );
    }
  }

  namespace sat {

    std::ostream & dumpOn( std::ostream & str, const Queue & obj )
    {
      str << "Queue {";
      if ( ! obj.empty() )
      {
        str << endl;
        for ( auto it = obj.begin(); it != obj.end(); ++it )
          str << "  " << Solvable(*it) << endl;
      }
      return str << "}";
    }

  } // namespace sat
} // namespace zyppng
