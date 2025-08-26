/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_SHARED_TVM_TVMSETTINGS_H_INCLUDED
#define ZYPP_SHARED_TVM_TVMSETTINGS_H_INCLUDED

#include <string>
#include <vector>
#include <yaml-cpp/node/node.h>

namespace zypp::test {

  struct TVMSettings
  {
     struct Device {
        std::string name;          // the device name
        std::string insertedPath;  // the path what is currently inserted into the device
     };
     std::vector<Device> devices;
  };
}

namespace YAML {
  template<>
  struct convert<zypp::test::TVMSettings> {
    static Node encode(const zypp::test::TVMSettings& rhs) {
      Node node;
      node["devices"] = rhs.devices;
      return node;
    }

    static bool decode(const Node& node, zypp::test::TVMSettings& rhs) {
      if(!node.IsMap() || !node["devices"] ) {
        return false;
      }

      rhs.devices = node["devices"].as<std::vector<zypp::test::TVMSettings::Device>>();
      return true;
    }
  };

  template<>
  struct convert<zypp::test::TVMSettings::Device> {
    static Node encode(const zypp::test::TVMSettings::Device& rhs) {
      Node node;
      node["name"] = rhs.name;
      node["insertedPath"] = rhs.insertedPath;
      return node;
    }

    static bool decode(const Node& node, zypp::test::TVMSettings::Device& rhs) {
      if(!node.IsMap() || !node["name"] || !node["insertedPath"]  ) {
        return false;
      }

      rhs.name = node["name"].as<std::string>();
      rhs.insertedPath = node["insertedPath"].as<std::string>();
      return true;
    }
  };
}


#endif
