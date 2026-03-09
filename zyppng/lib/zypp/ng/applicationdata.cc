/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "applicationdata.h"

#include <zypp-core/base/LogTools.h>


#undef ZYPP_BASE_LOGGER_LOGGROUP
#define ZYPP_BASE_LOGGER_LOGGROUP "zyppng::UserData"

namespace zyppng {
  bool ApplicationData::hasData()
  {
     return !instance()._value.empty();
  }

  const std::string &ApplicationData::data()
  {
    return instance()._value;
  }

  bool ApplicationData::setData( const std::string &str )
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

  ApplicationData &ApplicationData::instance()
  {
    static ApplicationData _inst;
    return _inst;
  }
}
