/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "private/error_p.h"
#include "private/context_p.h"
#include "private/progressobserver_p.h"
#include <zypp-core/zyppng/ui/progressobserver.h>
#include <zypp/ZYppFactory.h>
#include <zypp-core/zyppng/base/private/threaddata_p.h>
#include <zypp-core/base/String.h>

#include <zypp-glib/utils/RetainPtr>
#include <iostream>

typedef enum
{
  PROP_CPPOBJ  = 1,
  N_PROPERTIES
} ZyppContextProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

#if 0
/* Signals */
typedef enum {
    LAST_SIGNAL
} ZyppContextSignals;

static guint signals[LAST_SIGNAL] = { 0, };
#endif

G_DEFINE_TYPE_WITH_PRIVATE(ZyppContext, zypp_context, G_TYPE_OBJECT)

ZYPP_DECLARE_GOBJECT_BOILERPLATE( ZyppContext, zypp_context )
ZYPP_DEFINE_GOBJECT_BOILERPLATE ( ZyppContext, zypp_context, ZYPP, CONTEXT )

#define ZYPP_CONTEXT_D() \
  auto d = zypp_context_get_private(self)

void zypp_context_class_init( ZyppContextClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  ZYPP_INIT_GOBJECT_BOILERPLATE_KLASS( zypp_context, object_class );

  obj_properties[PROP_CPPOBJ] = ZYPP_GLIB_ADD_CPPOBJ_PROP();

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     obj_properties);
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
    _context = zyppng::AsyncContext::create();
  }
  _constructProps.reset();

  _signalConns.insert ( _signalConns.end(), {
    _context->connectFunc( &zyppng::ContextBase::sigProgressObserverChanged, [this]() {
      if ( _context->progressObserver () )
        _masterProgress = zypp::glib::zypp_wrap_cpp<ZyppProgressObserver>( _context->progressObserver ());
      else
        _masterProgress = nullptr;
    })
  });

  if ( _context->progressObserver () )
    _masterProgress = zypp::glib::zypp_wrap_cpp<ZyppProgressObserver>( _context->progressObserver ());

}

zyppng::AsyncContextRef &ZyppContextPrivate::cppType()
{
  return _context;
}

static void
zypp_context_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  g_return_if_fail( ZYPP_IS_CONTEXT(object) );
  ZyppContext *self = ZYPP_CONTEXT (object);

  ZYPP_CONTEXT_D();

  switch ((ZyppContextProperty)property_id )
    {
    case PROP_CPPOBJ:
      g_return_if_fail( d->_constructProps ); // only if the constr props are still valid
      ZYPP_GLIB_SET_CPPOBJ_PROP( zyppng::AsyncContextRef, value, d->_constructProps->_cppObj )
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
  g_return_if_fail( ZYPP_IS_CONTEXT(object) );

  switch ((ZyppContextProperty)property_id )
    {
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

zyppng::AsyncContextRef zypp_context_get_cpp( ZyppContext *self )
{
  g_return_val_if_fail( ZYPP_CONTEXT(self), nullptr );
  ZYPP_CONTEXT_D();
  return d->_context;
}

gboolean zypp_context_load_system (ZyppContext *self, const gchar *sysRoot , GError **error)
{
  g_return_val_if_fail( ZYPP_CONTEXT(self), false );
  ZYPP_CONTEXT_D();

  zyppng::ContextSettings set;
  if ( sysRoot )
    set.root = zypp::asString(sysRoot);

  try {
    d->_context->initialize( std::move(set) ).unwrap();
    return true;
  } catch ( ... ) {
    zypp_error_from_exception ( error, std::current_exception () );
  }

  return false;
}

gchar * zypp_context_sysroot ( ZyppContext *self )
{
  g_return_val_if_fail( ZYPP_CONTEXT(self), nullptr );
  ZYPP_CONTEXT_D();
  return g_strdup ( d->_context->contextRoot().c_str() );
}

ZyppProgressObserver *zypp_context_get_progress_observer( ZyppContext *self )
{
  g_return_val_if_fail( ZYPP_CONTEXT(self), nullptr );
  ZYPP_CONTEXT_D();
  return d->_masterProgress.get();
}

void zypp_context_set_progress_observer( ZyppContext *self, ZyppProgressObserver *obs )
{
  g_return_if_fail( ZYPP_CONTEXT(self) );
  ZYPP_CONTEXT_D();
  if ( obs )
    d->_context->setProgressObserver ( zypp_progress_observer_get_cpp (obs) );
  else
    d->_context->resetProgressObserver ();
}

void zypp_context_reset_progress_observer( ZyppContext *self )
{
  g_return_if_fail( ZYPP_CONTEXT(self) );
  ZYPP_CONTEXT_D();
  d->_context->resetProgressObserver();
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
