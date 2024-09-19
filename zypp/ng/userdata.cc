/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "userdata.h"

#include <zypp/base/LogTools.h>


#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zyppng::UserData"

namespace zyppng {
  bool UserData::hasData()
  {
     return !instance()._value.empty();
  }

  const std::string &UserData::data()
  {
    return instance()._value;
  }

  bool UserData::setData( const std::string &str )
  {
    for( auto ch = str.begin(); ch != str.end(); ch++ )
    {
      if ( *ch < ' ' && *ch != '\t' )
      {
        ERR << "New user data string rejectded: char " << (int)*ch << " at position " <<  (ch - str.begin()) << std::endl;
        return false;
      }
    }
    MIL << "Set user data string to '" << str << "'" << std::endl;
    instance()._value = str;
    return true;
  }

  UserData &UserData::instance()
  {
    static UserData _inst;
    return _inst;
  }
}
