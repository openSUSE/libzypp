/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "solvablespec.h"

#include <iostream>
#include <zypp/ng/sat/solvablespec.h>

namespace zyppng {

  namespace log {

    std::ostream & formatter<sat::SolvableSpec>::stream( std::ostream & str, const sat::SolvableSpec & obj )
    {
      str << "SolvableSpec{idents:[";
      bool first = true;
      for ( const auto & id : obj.idents() ) {
        if ( !first ) str << ',';
        str << id.c_str();
        first = false;
      }
      str << "] provides:[";
      first = true;
      for ( const auto & cap : obj.provides() ) {
        if ( !first ) str << ',';
        str << "provides:" << cap.c_str();
        first = false;
      }
      return str << "]}";
    }

    std::ostream & formatter<sat::EvaluatedSolvableSpec>::stream( std::ostream & str, const sat::EvaluatedSolvableSpec & obj )
    {
      return str << "EvaluatedSolvableSpec{ids:" << obj.ids().size() << "}";
    }

  } // namespace log

} // namespace zyppng
