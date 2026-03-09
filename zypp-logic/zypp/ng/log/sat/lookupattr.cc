/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------*/
#include "lookupattr.h"
#include <iostream>

#include <zypp/ng/sat/repository.h>
#include <zypp/ng/sat/solvable.h>
#include <zypp/ng/idstring.h>

#include <zypp/ng/log/sat/repository.h>
#include <zypp/ng/log/sat/solvable.h>

extern "C"
{
#include <solv/pool.h>
}

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

    if ( obj.inSolvable() )
      str << obj.inSolvable();
    else if ( obj.inRepo() )
      str << obj.inRepo();

    str << '<' << obj.inSolvAttr() << (obj.solvAttrSubEntry() ? ">(*" : ">(")
        <<  IdString(obj.solvAttrType()) << ") = " << obj.asString();
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
