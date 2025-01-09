/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "application.h"
#include "private/globals_p.h"

#include <zypp-core/zyppng/base/eventdispatcher.h>
#include <zypp-core/zyppng/base/private/threaddata_p.h>
#include <zypp/ng/userdata.h>


static void glogHandler (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);
static gpointer installLogHandler ( gpointer );

struct ZyppApplicationPrivate
{
  ZyppApplicationPrivate( ZyppApplication *pub ) {
    static GOnce my_once = G_ONCE_INIT;
    g_once( &my_once, installLogHandler, nullptr );
  }

  void initialize() {
    if ( !_dispatcher ) {
      _dispatcher = zyppng::ThreadData::current().ensureDispatcher();
    }
  }

  void finalize() { /* Nothing to do atm */}


  ZyppApplication *_gObject = nullptr;
  zyppng::EventDispatcherRef _dispatcher;
  std::string version = LIBZYPP_VERSION_STRING;
};

G_DEFINE_TYPE_WITH_PRIVATE( ZyppApplication, zypp_application, G_TYPE_OBJECT )

ZYPP_DECLARE_GOBJECT_BOILERPLATE( ZyppApplication, zypp_application )
ZYPP_DEFINE_GOBJECT_BOILERPLATE ( ZyppApplication, zypp_application, ZYPP, APPLICATION )

#define ZYPP_APPLICATION_D() \
  auto d = zypp_application_get_private( self )

typedef enum
{
  PROP_EVENT_DISPATCHER  = 1,
  PROP_VERSION,
  N_PROPERTIES
} ZyppApplicationProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

void zypp_application_class_init( ZyppApplicationClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  ZYPP_INIT_GOBJECT_BOILERPLATE_KLASS( zypp_application, object_class );

  obj_properties[PROP_EVENT_DISPATCHER] =
  g_param_spec_boxed ( "eventDispatcher",
                        "EventDispatcher",
                        "Libzypp Event Dispatcher.",
                        G_TYPE_MAIN_CONTEXT,
                        GParamFlags( G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE ) );

  obj_properties[PROP_VERSION] =
  g_param_spec_string ( "version",
                        "Version",
                        "Libzypp Library version.",
                        NULL  /* default value */,
                        GParamFlags( G_PARAM_READABLE) );

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     obj_properties);
}

static void
zypp_application_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  g_return_if_fail( ZYPP_IS_APPLICATION(object) );
  ZyppApplication *self = ZYPP_APPLICATION (object);

  ZYPP_APPLICATION_D();

  switch ((ZyppApplicationProperty)property_id )
    {
    case PROP_EVENT_DISPATCHER: {
      GMainContext *ctx = reinterpret_cast<GMainContext *>(g_value_get_boxed( value ));

      // this will take a reference of the context
      if ( !d->_dispatcher ) d->_dispatcher = zyppng::ThreadData::current().ensureDispatcher( ctx );
      else MIL << "Ignoring GMainContext, dispatcher already initialized!" << std::endl;

      break;
    }
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
zypp_application_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  g_return_if_fail( ZYPP_IS_APPLICATION(object) );
  ZyppApplication *self = ZYPP_APPLICATION (object);

  ZYPP_APPLICATION_D();

  switch ((ZyppApplicationProperty)property_id )
    {
    case PROP_EVENT_DISPATCHER:
      g_value_set_boxed( value, d->_dispatcher->nativeDispatcherHandle() );
      break;
    case PROP_VERSION:
      g_value_set_string ( value, d->version.c_str() );
      break;
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

ZyppApplication *zypp_application_new( GMainContext *eventDispatcher )
{
  if ( eventDispatcher ){
    GValue dispValue = G_VALUE_INIT;
    g_value_init( &dispValue, G_TYPE_MAIN_CONTEXT );
    g_value_set_boxed ( &dispValue, eventDispatcher );
    return static_cast<ZyppApplication *>( g_object_new( ZYPP_TYPE_APPLICATION, "eventDispatcher", &dispValue, nullptr ) );
  } else {
    return static_cast<ZyppApplication *>( g_object_new( ZYPP_TYPE_APPLICATION, nullptr ) );
  }
}

GMainContext *zypp_application_get_dispatcher( ZyppApplication *self )
{
  g_return_val_if_fail ( ZYPP_IS_APPLICATION(self), nullptr );
  ZYPP_APPLICATION_D();
  return d->_dispatcher->glibContext();
}

const gchar * zypp_application_get_version ( ZyppApplication *self )
{
  g_return_val_if_fail( ZYPP_IS_APPLICATION(self), nullptr );
  ZYPP_APPLICATION_D();
  return d->version.c_str();
}

void zypp_application_set_user_data (ZyppApplication *self, const gchar *userData )
{
  if ( !userData ) {
    zyppng::UserData::setData( std::string() );
    return;
  }
  zyppng::UserData::setData ( zypp::str::asString (userData) );
}

gboolean zypp_application_has_user_data ( ZyppApplication *self )
{
  return zyppng::UserData::hasData();
}

const gchar *zypp_application_get_user_data ( ZyppApplication *self )
{
  return zyppng::UserData::data().c_str();
}


static gpointer installLogHandler ( gpointer )
{
  g_log_set_default_handler( glogHandler, nullptr );
  return nullptr;
}

void glogHandler (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer )
{
  switch (log_level & G_LOG_LEVEL_MASK) {
    case G_LOG_LEVEL_ERROR:
    case G_LOG_LEVEL_CRITICAL:
      ERR << log_domain << ":" << message << std::endl;
      break;
    case G_LOG_LEVEL_WARNING:
      WAR << log_domain << ":" << message << std::endl;
      break;
    case G_LOG_LEVEL_MESSAGE:
      MIL << log_domain << ":" << message << std::endl;
      break;
    case G_LOG_LEVEL_INFO:
      XXX << log_domain << ":" << message << std::endl;
      break;
    case G_LOG_LEVEL_DEBUG:
      DBG << log_domain << ":" << message << std::endl;
      break;
    default:
      MIL << log_domain << "("<<(log_level & G_LOG_LEVEL_MASK)<<"):" << message << std::endl;
      break;
  }
}
