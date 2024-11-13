/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/showmessagerequest_p.h"
#include "private/userrequest_p.h"

#include <zypp-glib/private/globals_p.h>
#include <zypp-glib/ui/zyppenums-ui.h>


static void user_request_if_init( ZyppUserRequestInterface *iface );

ZYPP_DECLARE_GOBJECT_BOILERPLATE ( ZyppShowMsgRequest, zypp_show_msg_request )

G_DEFINE_TYPE_WITH_CODE(ZyppShowMsgRequest, zypp_show_msg_request, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE ( ZYPP_TYPE_USER_REQUEST, user_request_if_init )
  G_ADD_PRIVATE ( ZyppShowMsgRequest )
)

ZYPP_DEFINE_GOBJECT_BOILERPLATE ( ZyppShowMsgRequest, zypp_show_msg_request, ZYPP, SHOW_MSG_REQUEST )

#define ZYPP_D() \
  ZYPP_GLIB_WRAPPER_D( ZyppShowMsgRequest, zypp_show_msg_request )


// define the GObject stuff
enum {
  PROP_CPPOBJ  = 1,
  PROP_LABEL,
  PROP_TYPE,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void user_request_if_init( ZyppUserRequestInterface *iface )
{
  ZyppUserRequestImpl<ZyppShowMsgRequest, zypp_show_msg_request_get_type, zypp_show_msg_request_get_private >::init_interface( iface );
}

static void zypp_show_msg_request_class_init (ZyppShowMsgRequestClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ZYPP_INIT_GOBJECT_BOILERPLATE_KLASS ( zypp_show_msg_request, gobject_class );

  obj_properties[PROP_CPPOBJ] = ZYPP_GLIB_ADD_CPPOBJ_PROP();
  obj_properties[PROP_LABEL] = g_param_spec_string (
        "label",
        "Label",
        "String for the message Label.",
        NULL  /* default value */,
        GParamFlags( G_PARAM_CONSTRUCT | G_PARAM_READWRITE ) );

  obj_properties[PROP_TYPE] = g_param_spec_enum (
        "type",
        "Type",
        "Message type specifier.",
        zypp_show_msg_request_type_get_type(),
        ZYPP_SHOW_MSG_TYPE_INFO,
        GParamFlags( G_PARAM_CONSTRUCT | G_PARAM_READWRITE ) );

  g_object_class_install_properties (gobject_class,
                                     N_PROPERTIES,
                                     obj_properties);
}

static void
zypp_show_msg_request_get_property (GObject    *object,
  guint       property_id,
  GValue     *value,
  GParamSpec *pspec)
{
  g_return_if_fail( ZYPP_IS_SHOW_MSG_REQUEST (object) );
  ZyppShowMsgRequest *self = ZYPP_SHOW_MSG_REQUEST (object);

  ZYPP_D();

  switch (property_id)
  {
    case PROP_LABEL: {
      if ( d->_constrProps ) {
        if ( d->_constrProps->_label )
          g_value_set_string( value, d->_constrProps->_label->c_str() );
      } else {
        g_return_if_fail( d->_cppObj );
        g_value_set_string( value, d->_cppObj->message().c_str() );
        return;
      }
      break;
    }
    case PROP_TYPE: {
      if ( d->_constrProps ) {
        if ( d->_constrProps->_type )
          g_value_set_enum( value, static_cast<int>(*d->_constrProps->_type) );
      } else {
        g_return_if_fail( d->_cppObj );
        g_value_set_enum( value, static_cast<int>( d->convertEnum( d->_cppObj->messageType() )));
        return;
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
zypp_show_msg_request_set_property (GObject      *object,
  guint         property_id,
  const GValue *value,
  GParamSpec   *pspec)
{
  g_return_if_fail( ZYPP_IS_SHOW_MSG_REQUEST (object) );
  ZyppShowMsgRequest *self = ZYPP_SHOW_MSG_REQUEST (object);

  ZYPP_D();

  switch (property_id)
  {
    case PROP_CPPOBJ: {
      g_return_if_fail( d->_constrProps ); // only if the constr props are still valid
      ZYPP_GLIB_SET_CPPOBJ_PROP( zyppng::ShowMessageRequestRef, value, d->_constrProps->_cppObj );
      break;
    }
    case PROP_LABEL: {
      if ( d->_constrProps ) {
        d->_constrProps->_label = zypp::str::asString( g_value_get_string(value) );
      } else {
        g_warning ("The label property is only writable at construction.");
        return;
      }
      break;
    }
    case PROP_TYPE: {
      if ( d->_constrProps ) {
        d->_constrProps->_type = static_cast<ZyppShowMsgRequestType>(g_value_get_enum ( value ));
      } else {
        g_warning ("The type property is only writable at construction.");
        return;
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

ZyppShowMsgRequestType zypp_show_msg_request_get_message_type ( ZyppShowMsgRequest *self )
{
  ZYPP_D();
  g_return_val_if_fail ( d->_cppObj, ZYPP_SHOW_MSG_TYPE_INFO );
  return  d->convertEnum(d->_cppObj->messageType());
}

const char * zypp_show_msg_request_get_message( ZyppShowMsgRequest *self )
{
  ZYPP_D();
  g_return_val_if_fail ( d->_cppObj, nullptr );
  return d->_cppObj->message().data();
}

zyppng::ShowMessageRequestRef &zypp_show_message_request_get_cpp( ZyppShowMsgRequest *self )
{
  ZYPP_D();
  return d->_cppObj;
}
