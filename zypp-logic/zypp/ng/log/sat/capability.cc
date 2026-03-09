/*---------------------------------------------------------------------
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "capability.h"

#include <iostream>
#include <zypp-core/base/LogTools.h>
#include <zypp-core/base/String.h>
#include <zypp/ng/sat/capability.h>
#include <zypp/ng/sat/capabilities.h>
#include <zypp/ng/sat/stringpool.h>
#include <zypp/ng/sat/cap2str.h>

extern "C"
{
#include <solv/pool.h>
}

namespace zyppng {

  namespace log {
    std::ostream &formatter<sat::Capability>::stream(std::ostream &str, const sat::Capability &obj)
    {
      return str << obj.c_str();
    }

    std::ostream &formatter<sat::CapDetail>::stream(std::ostream &str, const sat::CapDetail &obj)
    {
      static const char archsep = '.';
      switch ( obj.kind() )
      {
        case sat::CapDetail::NOCAP:
          return str << "<NoCap>";
          break;

        case sat::CapDetail::NAMED:
          str << obj.name();
          if ( obj.hasArch() )
            str << archsep << obj.arch();
          return str;
          break;

        case sat::CapDetail::VERSIONED:
          str << obj.name();
          if ( obj.hasArch() )
            str << archsep << obj.arch();
          return str << " " << obj.op() << " " << obj.ed();
          break;

        case sat::CapDetail::EXPRESSION:
        {
          std::string outs;
          auto pool = sat::StringPool::instance().getPool();
          auto op = obj.capRel();
          if ( obj.capRel() == sat::CapDetail::CAP_NAMESPACE ) {
            sat::detail::cap2str( outs, pool, obj.lhs().id(), op );
            outs += "(";
            sat::detail::cap2str( outs, pool, obj.rhs().id(), op );
            outs += ")";
          }
          else {
            outs += "(";
            sat::detail::cap2str( outs, pool, obj.lhs().id(), op );
            outs += sat::detail::capRel2Str( op );
            sat::detail::cap2str( outs, pool, obj.rhs().id(), op );
            outs += ")";
          }
          return str << outs;
        }
        break;
      }
      return str <<  "<UnknownCap(" << obj.lhs() << " " << obj.capRel() << " " << obj.rhs() << ")>";
    }

    std::ostream &zyppng::log::formatter<sat::CapDetail::Kind>::stream(std::ostream &str, const sat::CapDetail::Kind &obj)
    {
      switch ( obj )
      {
        case sat::CapDetail::NOCAP:      return str << "NoCap"; break;
        case sat::CapDetail::NAMED:      return str << "NamedCap"; break;
        case sat::CapDetail::VERSIONED:  return str << "VersionedCap"; break;
        case sat::CapDetail::EXPRESSION: return str << "CapExpression"; break;
      }
      return str << "UnknownCap";
    }

    std::ostream &zyppng::log::formatter<sat::CapDetail::CapRel>::stream(std::ostream &str, const sat::CapDetail::CapRel &obj)
    {
      switch ( obj )
      {
        case sat::CapDetail::REL_NONE:      return str << "NoCapRel"; break;
        case sat::CapDetail::CAP_AND:       return str << "and"; break;        // &
        case sat::CapDetail::CAP_OR:        return str << "or"; break;         // |
        case sat::CapDetail::CAP_COND:      return str << "if"; break;
        case sat::CapDetail::CAP_UNLESS:    return str << "unless"; break;
        case sat::CapDetail::CAP_ELSE:      return str << "else"; break;
        case sat::CapDetail::CAP_WITH:      return str << "with"; break;       // +
        case sat::CapDetail::CAP_WITHOUT:   return str << "without"; break;    // -
        case sat::CapDetail::CAP_NAMESPACE: return str << "NAMESPACE"; break;
        case sat::CapDetail::CAP_ARCH:      return str << "ARCH"; break;
     }
     return str << "UnknownCapRel("+zypp::str::numstring(obj)+")";
    }
  }
} // namespace zyppng
