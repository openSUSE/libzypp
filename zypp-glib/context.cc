/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "error.h"
#include "private/context_p.h"
#include "private/repomanager_p.h"
#include <zypp/ZYppFactory.h>
#include <zypp-core/zyppng/base/private/threaddata_p.h>
#include <zypp-core/base/String.h>

#include <zypp-glib/utils/RetainPtr>

#include <zypp/ng/userrequest.h>
#include <zypp-glib/ui/userrequest.h>
#include <zypp-glib/ui/booleanchoicerequest.h>
#include <zypp-glib/ui/listchoicerequest.h>
#include <zypp-glib/ui/showmessagerequest.h>

#include <iostream>

typedef enum
{
  PROP_CPPOBJ  = 1,
  PROP_VERSION,
  N_PROPERTIES
} ZyppContextProperty;

GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };


/* Signals */
typedef enum {
    SIG_EVENT = 1,
    LAST_SIGNAL
} ZyppContextSignals;

static guint signals[LAST_SIGNAL] = { 0, };


G_DEFINE_TYPE_WITH_PRIVATE(ZyppContext, zypp_context, G_TYPE_OBJECT)

ZYPP_DECLARE_GOBJECT_BOILERPLATE( ZyppContext, zypp_context )
ZYPP_DEFINE_GOBJECT_BOILERPLATE ( ZyppContext, zypp_context, ZYPP, CONTEXT )

#define ZYPP_CONTEXT_D() \
  ZYPP_GLIB_WRAPPER_D( ZyppContext, zypp_context )

void zypp_context_class_init( ZyppContextClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  ZYPP_INIT_GOBJECT_BOILERPLATE_KLASS( zypp_context, object_class );

  obj_properties[PROP_CPPOBJ] = ZYPP_GLIB_ADD_CPPOBJ_PROP();

  obj_properties[PROP_VERSION] =
  g_param_spec_string ("versionprop",
                        "Version",
                        "Libzypp Library version.",
                        NULL  /* default value */,
                        GParamFlags( G_PARAM_READABLE) );

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     obj_properties);

  {
    std::vector<GType> signal_parms = { ZYPP_TYPE_USER_REQUEST };
    signals[SIG_EVENT] =
      g_signal_newv ("event",
                     G_TYPE_FROM_CLASS (klass),
                    (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS),
                    NULL /* closure */,
                    NULL /* accumulator */,
                    NULL /* accumulator data */,
                    NULL /* C marshaller */,
                    G_TYPE_NONE /* return_type */,
                    signal_parms.size() /* n_params */,
                    signal_parms.data() );
  }


}

static void glogHandler (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);
static gpointer installLogHandler ( gpointer );

void ZyppContextPrivate::initializeCpp()
{
  static GOnce my_once = G_ONCE_INIT;
  g_once( &my_once, installLogHandler, nullptr );

  if ( _constructProps && _constructProps->_cppObj ) {
    _context = std::move( _constructProps->_cppObj );
  } else {
    _context = zyppng::SyncContext::create();
  }
  _constructProps.reset();

  _signalConns.insert ( _signalConns.end(), {
     _context->connectFunc( &zyppng::ContextBase::sigEvent, [this]( zyppng::UserRequestRef req ) {
          switch( req->type() )  {
            case zyppng::UserRequestType::Invalid: {
              ERR << "Ignoring user request with type Invalid, this is a bug!" << std::endl;
              return; //ignore
            }
            case zyppng::UserRequestType::Message: {
              auto actualReq = std::dynamic_pointer_cast<zyppng::ShowMessageRequest>(req);
              auto gObjMsg = zypp::glib::zypp_wrap_cpp<ZyppShowMsgRequest>(actualReq);
              g_signal_emit (_gObject, signals[SIG_EVENT], 0, gObjMsg.get(), nullptr );
              return;
            }
            case zyppng::UserRequestType::ListChoice: {
              auto actualReq = std::dynamic_pointer_cast<zyppng::ListChoiceRequest>(req);
              auto gObjMsg = zypp::glib::zypp_wrap_cpp<ZyppListChoiceRequest>(actualReq);
              g_signal_emit (_gObject, signals[SIG_EVENT], 0, gObjMsg.get(), nullptr );
              return;
            }
            case zyppng::UserRequestType::BooleanChoice: {
              auto actualReq = std::dynamic_pointer_cast<zyppng::BooleanChoiceRequest>(req);
              auto gObjMsg = zypp::glib::zypp_wrap_cpp<ZyppBooleanChoiceRequest>(actualReq);
              g_signal_emit (_gObject, signals[SIG_EVENT], 0, gObjMsg.get(), nullptr );
              return;
            }
            case zyppng::UserRequestType::Custom: {
              ERR << "Custom user requests can not be wrapped with glib!" << std::endl;
              return; //ignore
            }
          }
     })
  });

}

zyppng::SyncContextRef &ZyppContextPrivate::cppType()
{
  return _context;
}

static void
zypp_context_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ZyppContext *self = ZYPP_CONTEXT (object);
  g_return_if_fail( ZYPP_IS_CONTEXT(object) );

  ZYPP_CONTEXT_D();

  switch ((ZyppContextProperty)property_id )
    {
    case PROP_CPPOBJ:
      g_return_if_fail( d->_constructProps ); // only if the constr props are still valid
      ZYPP_GLIB_SET_CPPOBJ_PROP( zyppng::SyncContextRef, value, d->_constructProps->_cppObj )
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
zypp_context_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  ZyppContext *self = ZYPP_CONTEXT (object);
  g_return_if_fail( ZYPP_IS_CONTEXT(object) );

  ZYPP_CONTEXT_D();

  switch ((ZyppContextProperty)property_id )
    {
    case PROP_VERSION:
      g_value_set_string ( value, d->version.c_str() );
      break;
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

zyppng::SyncContextRef zypp_context_get_cpp( ZyppContext *self )
{
  ZYPP_CONTEXT_D();
  return d->_context;
}

gboolean zypp_context_load_system (ZyppContext *self, const gchar *sysRoot , GError **error)
{
  ZYPP_CONTEXT_D();

  zyppng::ContextSettings set;
  if ( sysRoot )
    set.root = zypp::asString(sysRoot);

  try {
    d->_context->initialize( std::move(set) ).unwrap();
    return true;
  } catch ( const zypp::Exception &e ) {
    if ( error ) {
      g_error_new( ZYPP_EXCEPTION, ZYPP_ERROR, "%s", e.asString().c_str() );
    }
  } catch ( const std::exception &e ) {
    if ( error ) {
      g_error_new( ZYPP_EXCEPTION, ZYPP_ERROR, "%s", e.what() );
    }
  } catch ( ... ) {
    if ( error ) {
      g_error_new( ZYPP_EXCEPTION, ZYPP_ERROR, "%s", "Unknown exception." );
    }
  }

  return false;
}

const gchar * zypp_context_version ( ZyppContext *self )
{
  ZYPP_CONTEXT_D();
  return d->version.c_str();
}

gchar * zypp_context_sysroot ( ZyppContext *self )
{
  ZYPP_CONTEXT_D();
  return g_strdup ( d->_context->contextRoot().c_str() );
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
