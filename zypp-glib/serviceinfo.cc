/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/serviceinfo_p.h"
#include "private/infobase_p.h"
#include "zypp-glib/private/context_p.h"


ZYPP_DECLARE_GOBJECT_BOILERPLATE ( ZyppServiceInfo, zypp_service_info )
static void zypp_info_base_interface_init( ZyppInfoBaseInterface *iface );

G_DEFINE_TYPE_WITH_CODE(ZyppServiceInfo, zypp_service_info, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE ( ZYPP_TYPE_INFO_BASE, zypp_info_base_interface_init)
  G_ADD_PRIVATE ( ZyppServiceInfo )
)

#define ZYPP_SERVICE_INFO_D() \
  ZYPP_GLIB_WRAPPER_D( ZyppServiceInfo, zypp_service_info )


enum {
  PROP_0,
  PROP_CPPOBJ,
  PROP_CONTEXT,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

ZYPP_DEFINE_GOBJECT_BOILERPLATE ( ZyppServiceInfo, zypp_service_info, ZYPP, SERVICE_INFO )

static void zypp_info_base_interface_init( ZyppInfoBaseInterface *iface )
{
  ZyppInfoBaseImpl<ZyppServiceInfo, zypp_service_info_get_type, zypp_service_info_get_private >::init_interface( iface );
}

static void
zypp_service_info_class_init (ZyppServiceInfoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ZYPP_INIT_GOBJECT_BOILERPLATE_KLASS ( zypp_service_info, gobject_class );

  obj_properties[PROP_CPPOBJ] = ZYPP_GLIB_ADD_CPPOBJ_PROP();
}

static void
zypp_service_info_get_property (GObject    *object,
  guint       property_id,
  GValue     *value,
  GParamSpec *pspec)
{
  switch (property_id)
  {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
zypp_service_info_set_property (GObject      *object,
  guint         property_id,
  const GValue *value,
  GParamSpec   *pspec)
{
  ZyppServiceInfo *self = ZYPP_SERVICE_INFO (object);
  ZYPP_SERVICE_INFO_D();

  switch (property_id)
  {
    case PROP_CPPOBJ:
      g_return_if_fail( d->_constrProps ); // only if the constr props are still valid
      ZYPP_GLIB_SET_CPPOBJ_PROP( zyppng::ServiceInfo, value, d->_constrProps->_cppObj )
    case PROP_CONTEXT: {
      g_return_if_fail( d->_constrProps ); // only if the constr props are still valid
      ZyppContext *obj = ZYPP_CONTEXT(g_value_get_object( value ));
      g_return_if_fail( ZYPP_IS_CONTEXT(obj) );
      if ( d->_constrProps ) {
        d->_constrProps->_context = zypp::glib::ZyppContextRef( obj, zypp::glib::retain_object );
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void ZyppServiceInfoPrivate::initialize()
{
  g_return_if_fail ( _constrProps.has_value() );
  if ( _constrProps->_cppObj ) {
    _info = std::move( _constrProps->_cppObj.value() );
  } else {
    if ( !_constrProps->_context ) g_error("Context argument can not be NULL");
    _info = zyppng::ServiceInfo( zypp_context_get_cpp( _constrProps->_context.get() ) );
  }
  _constrProps.reset();
}

ZyppServiceInfo *zypp_service_info_new ( ZyppContext *context )
{
  g_return_val_if_fail( context != nullptr, nullptr );
  return static_cast<ZyppServiceInfo *>( g_object_new (ZYPP_TYPE_SERVICE_INFO, "zyppcontext", context, NULL) );
}

ZyppServiceInfo *zypp_wrap_cpp( zyppng::ServiceInfo info )
{
  return ZYPP_SERVICE_INFO(g_object_new (ZYPP_TYPE_SERVICE_INFO, zypp::glib::internal::ZYPP_CPP_OBJECT_PROPERTY_NAME.data(), &info, NULL));
}
