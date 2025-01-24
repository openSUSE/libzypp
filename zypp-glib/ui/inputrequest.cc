/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/inputrequest_p.h"
#include "private/userrequest_p.h"

#include <zypp-glib/private/globals_p.h>
#include <zypp-glib/ui/zyppenums-ui.h>

ZyppInputRequestField * zypp_wrap_cpp( zyppng::InputRequest::Field obj )
{
  std::unique_ptr<ZyppInputRequestField> ptr( new ZyppInputRequestField() );
  ptr->_cppObj = std::move(obj);
  return ptr.release();
}

ZyppInputRequestField *zypp_input_request_field_copy(ZyppInputRequestField *r)
{
  return zypp_wrap_cpp(r->_cppObj);
}

void zypp_input_request_field_free(ZyppInputRequestField *r)
{
  delete r;
}

ZyppInputRequestFieldType zypp_input_request_field_get_type(ZyppInputRequestField *field)
{
  return (ZyppInputRequestFieldType)field->_cppObj.type;
}

const char *zypp_input_request_field_get_label(ZyppInputRequestField *field)
{
  return field->_cppObj.label.c_str ();
}

const char *zypp_input_request_field_get_value(ZyppInputRequestField *field)
{
  return field->_cppObj.value.c_str ();
}


G_DEFINE_BOXED_TYPE (ZyppInputRequestField, zypp_input_request_field,
                     zypp_input_request_field_copy,
                     zypp_input_request_field_free)


static void user_request_if_init( ZyppUserRequestInterface *iface );

ZYPP_DECLARE_GOBJECT_BOILERPLATE ( ZyppInputRequest, zypp_input_request )

G_DEFINE_TYPE_WITH_CODE( ZyppInputRequest, zypp_input_request, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE ( ZYPP_TYPE_USER_REQUEST, user_request_if_init )
  G_ADD_PRIVATE ( ZyppInputRequest )
)

ZYPP_DEFINE_GOBJECT_BOILERPLATE ( ZyppInputRequest, zypp_input_request, ZYPP, INPUT_REQUEST )

#define ZYPP_D() \
  ZYPP_GLIB_WRAPPER_D( ZyppInputRequest, zypp_input_request )

// define the GObject stuff
enum {
  PROP_CPPOBJ  = 1,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void user_request_if_init( ZyppUserRequestInterface *iface )
{
  ZyppUserRequestImpl<ZyppInputRequest, zypp_input_request_get_type, zypp_input_request_get_private >::init_interface( iface );
}

static void zypp_input_request_class_init (ZyppInputRequestClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ZYPP_INIT_GOBJECT_BOILERPLATE_KLASS ( zypp_input_request, gobject_class );
  obj_properties[PROP_CPPOBJ] = ZYPP_GLIB_ADD_CPPOBJ_PROP();

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     obj_properties);
}

static void
zypp_input_request_get_property (GObject    *object,
  guint       property_id,
  GValue     *value,
  GParamSpec *pspec)
{
  g_return_if_fail( ZYPP_IS_INPUT_REQUEST (object) );
  switch (property_id)
  {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
zypp_input_request_set_property (GObject      *object,
  guint         property_id,
  const GValue *value,
  GParamSpec   *pspec)
{
  g_return_if_fail( ZYPP_IS_INPUT_REQUEST (object) );
  ZyppInputRequest *self = ZYPP_INPUT_REQUEST (object);

  ZYPP_D();

  switch (property_id)
  {
    case PROP_CPPOBJ: {
      g_return_if_fail( d->_constrProps ); // only if the constr props are still valid
      ZYPP_GLIB_SET_CPPOBJ_PROP( zyppng::InputRequestRef, value, d->_constrProps->_cppObj );
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

zyppng::InputRequestRef &zypp_input_request_get_cpp( ZyppInputRequest *self )
{
  ZYPP_D();
  return d->_cppObj;
}

const char * zypp_input_request_get_label( ZyppInputRequest *self )
{
  ZYPP_D();
  return d->_cppObj->label().c_str();
}

GList *zypp_input_request_get_fields( ZyppInputRequest *self )
{
  ZYPP_D();
  zypp::glib::GListContainer<ZyppInputRequestField> opts;
  for ( const auto &f : d->_cppObj->fields()) {
    opts.push_back ( zypp_wrap_cpp(f) );
  }
  return opts.take();
}

void zypp_input_request_set_field_value( ZyppInputRequest *self, guint fieldIndex, const char *value )
{
  ZYPP_D();
  g_return_if_fail( ( fieldIndex >= 0 && fieldIndex < d->_cppObj->fields ().size()) );
  d->_cppObj->fields ()[fieldIndex].value = zypp::str::asString(value);
}
