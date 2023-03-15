#include "private/serviceinfo_p.h"
#include "private/infobase_p.h"

static void zypp_info_base_interface_init( ZyppInfoBaseInterface *iface )
{
  ZyppInfoBaseImpl<ZyppServiceInfo, zypp_service_info_get_type >::init_interface( iface );
}

G_DEFINE_TYPE_WITH_CODE(ZyppServiceInfo, zypp_service_info, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE ( ZYPP_TYPE_INFO_BASE, zypp_info_base_interface_init) )


static void
zypp_service_info_finalize (GObject *object)
{
  auto *ptr = ZYPP_SERVICE_INFO (object);
  ptr->_data.~Cpp();

  G_OBJECT_CLASS (zypp_service_info_parent_class)->finalize (object);
}

static void
zypp_service_info_class_init (ZyppServiceInfoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = zypp_service_info_finalize;
}

static void
zypp_service_info_init (ZyppServiceInfo *self)
{
  new ( &self->_data ) ZyppServiceInfo::Cpp();
}

ZyppServiceInfo *zypp_service_info_new (void)
{
  return static_cast<ZyppServiceInfo *>( g_object_new (ZYPP_TYPE_SERVICE_INFO, NULL) );
}

ZyppServiceInfo *zypp_service_info_wrap_cpp(zypp::ServiceInfo &&info)
{
  auto self = zypp_service_info_new();
  self->_data._info = std::move(info);
  return self;
}
