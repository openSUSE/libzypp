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
extern "C"
{
#include <solv/pool.h>
}

module zyppng;

import :log_format;
import :log_sat_lookupattr;  // formatter<LookupAttr/DIWrap/iterator> specialization declarations
import :sat_lookupattr;
import :sat_repository;
import :sat_solvable;
import :idstring;
import :log_sat_repository;
import :log_sat_solvable;

namespace zyppng::log {

  std::ostream &formatter<sat::LookupAttr>::stream(std::ostream &str, const sat::LookupAttr &obj)
  {
    if ( obj.attr() == sat::SolvAttr::noAttr )
      return str << "search nothing";

    if ( obj.attr() )
      str << "search " << obj.attr() << " in ";
    else
      str << "search ALL in ";

    if ( obj.solvable() )
      return str << obj.solvable();
    if ( obj.repo() )
      return str << obj.repo();
    return str << "pool";
  }

#if 0
  std::ostream & dumpOn( std::ostream & str, const sat::LookupAttr & obj )
  {
    return zypp::dumpRange( str << obj, obj.begin(), obj.end() );
  }
#endif

  std::ostream &formatter<sat::detail::DIWrap>::stream(std::ostream &str, const sat::detail::DIWrap &obj)
  { return str << obj.get(); }


  std::ostream &formatter<sat::LookupAttr::iterator>::stream(std::ostream &str, const sat::LookupAttr::iterator &obj)
  {
    const sat::detail::CDataiterator * dip = obj.get();
    if ( ! dip )
      return str << "EndOfQuery";

    const auto val = *obj;   // cheap two-pointer proxy — valid while iterator is not advanced
    if ( val.inSolvable() )
      str << val.inSolvable();
    else if ( val.inRepo() )
      str << val.inRepo();

    str << '<' << val.inSolvAttr() << (val.solvAttrSubEntry() ? ">(*" : ">(")
        <<  IdString(val.solvAttrType()) << ") = " << val.asString();
    return str;
  }

  std::ostream &formatter<sat::detail::CDataiterator *>::stream(std::ostream &str, const sat::detail::CDataiterator *obj)
  {
    str << "detail::CDataiterator(";
    if ( ! obj )
    {
      str << "NULL";
    }
    else
    {
      str << "|" << sat::Repository(obj->repo);
      str << "|" << sat::Solvable(obj->solvid);
      str << "|" << IdString(obj->key->name);
      str << "|" << IdString(obj->key->type);
      str << "|" << obj->repodataid;
      str << "|" << obj->repoid;
    }
    return str << ")";
  }

}
