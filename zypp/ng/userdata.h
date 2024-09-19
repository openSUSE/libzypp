/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_USERDATA_H_INCLUDED
#define ZYPP_NG_USERDATA_H_INCLUDED

#include <string>

namespace zyppng {


  /**
   * Interface for usercode to define a string value to be passed to log, history, plugins...
   */
  class UserData {

  public:
    ~UserData() = default;
    static bool hasData();
    static const std::string &data();
    static bool setData( const std::string &str );

  private:
    UserData() = default;
    static UserData &instance();

    std::string _value;
  };

}

#endif
