/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
module;
#include <string>

export module zyppng:applicationdata;

export namespace zyppng {


  /**
   * Interface for usercode to define a string value to be passed to log, history, plugins...
   */
  class ApplicationData {

  public:
    ~ApplicationData() = default;
    static bool hasData();
    static const std::string &data();
    static bool setData( const std::string &str );

  private:
    ApplicationData() = default;
    static ApplicationData &instance();

    std::string _value;
  };

}

