/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_GLIB_PRIVATE_SERVICEINFO_P_H
#define ZYPP_GLIB_PRIVATE_SERVICEINFO_P_H

#include <zypp-glib/context.h>
#include <zypp-glib/serviceinfo.h>
#include <zypp-glib/private/globals_p.h>
#include <zypp/ng/serviceinfo.h>
#include <optional>

struct ZyppServiceInfoPrivate
{
  struct ConstructionProps {
    std::optional<zyppng::ServiceInfo> _cppObj;
    zypp::glib::ZyppContextRef _context =  nullptr;
  };
  std::optional<ConstructionProps> _constrProps = ConstructionProps();

  zyppng::ServiceInfo _info{ zyppng::ContextBaseRef(nullptr) };

  ZyppServiceInfoPrivate( ZyppServiceInfo *pub ) : _gObject(nullptr) {}
  void initialize();
  void finalize(){}

private:
  ZyppServiceInfo *_gObject = nullptr;
};

struct _ZyppServiceInfo {
  GObject parent_instance;
  struct Cpp {
    zyppng::ServiceInfo _info;
  } _data;
};


ZyppServiceInfo *zypp_wrap_cpp(zyppng::ServiceInfo info);


#endif // SERVICEINFO_P_H
