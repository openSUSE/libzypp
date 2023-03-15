/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPPNG_PRIVATE_SERVICEINFO_P_H
#define ZYPPNG_PRIVATE_SERVICEINFO_P_H

#include <zyppng/serviceinfo.h>
#include <zypp/ServiceInfo.h>


struct _ZyppServiceInfo {
  GObject parent_instance;
  struct Cpp {
    zypp::ServiceInfo _info;
  } _data;
};


ZyppServiceInfo *zypp_service_info_wrap_cpp(zypp::ServiceInfo &&info);


#endif // SERVICEINFO_P_H
