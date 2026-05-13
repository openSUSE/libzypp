/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
module;
#include <iostream>

module zyppng;

import :log_format;
import :log_sat_solvablespec;  // formatter<SolvableSpec/EvaluatedSolvableSpec> specialization declarations
import :sat_solvablespec;

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
