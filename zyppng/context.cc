/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/context_p.h"
#include "private/repomanager_p.h"
#include "private/downloader_p.h"
#include <zypp-core/zyppng/base/private/threaddata_p.h>
#include <zypp-core/base/String.h>

#include <iostream>

typedef enum
{
  PROP_CONTEXT = 1,
  PROP_VERSION,
  N_PROPERTIES
} ZyppContextProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };


/* Signals */

typedef enum {
    // FIRST_SIG = 1,
    LAST_SIGNAL
} ZyppContextSignals;

//static guint signals[LAST_SIGNAL] = { 0, };


G_DEFINE_TYPE(ZyppContext, zypp_context, G_TYPE_OBJECT)

static void zypp_context_dispose (GObject *gobject)
{
  /* In dispose(), you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a
   * reference.
   */

  // always call parent class dispose
  G_OBJECT_CLASS (zypp_context_parent_class)->dispose (gobject);
}

static void zypp_context_finalize (GObject *gobject)
{
  ZyppContext *ptr = ZYPP_CONTEXT(gobject);
  ptr->_data.~Cpp();

  // always call parent class finalizer
  G_OBJECT_CLASS (zypp_context_parent_class)->finalize (gobject);
}

static void
zypp_context_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ZyppContext *self = ZYPP_CONTEXT (object);
  g_return_if_fail( ZYPP_IS_CONTEXT(object) );

  switch ((ZyppContextProperty)property_id )
    {
    case PROP_CONTEXT: {
      GMainContext *boxed = reinterpret_cast<GMainContext *>(g_value_get_boxed( value ));
      self->_data._dispatcher = zyppng::ThreadData::current().ensureDispatcher(boxed);
      self->_data._downloader = zyppng::util::share_gobject( zypp_downloader_new( self ) );
      break;
    }
    case PROP_VERSION:
      self->_data.version = zypp::str::asString(static_cast<const char*>(g_value_get_string(value)));
      break;
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

  switch ((ZyppContextProperty)property_id )
    {
    case PROP_VERSION:
      g_value_set_string ( value, self->_data.version.c_str() );
      break;
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void glogHandler (const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);
static gpointer installLogHandler ( gpointer );


void zypp_context_class_init( ZyppContextClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = zypp_context_dispose;
  object_class->finalize = zypp_context_finalize;

  object_class->set_property = zypp_context_set_property;
  object_class->get_property = zypp_context_get_property;

  obj_properties[PROP_CONTEXT] =
  g_param_spec_boxed(  "glibContext",
                       "GLibContext",
                       "The glib context to use",
                       g_main_context_get_type(),
                       GParamFlags( G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE) );

  obj_properties[PROP_VERSION] =
  g_param_spec_string ("versionprop",
                        "Version",
                        "Libzypp Library version.",
                        NULL  /* default value */,
                        GParamFlags( G_PARAM_CONSTRUCT | G_PARAM_READWRITE) );

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     obj_properties);
}

void zypp_context_init( ZyppContext *ctx )
{
  static GOnce my_once = G_ONCE_INIT;
  g_once( &my_once, installLogHandler, nullptr );

  std::cout << "Context init func" << std::endl;
  new ( &ctx->_data ) ZyppContext::Cpp();
}

gboolean zypp_context_load_system ( ZyppContext *self, const gchar *sysRoot )
{
  g_return_val_if_fail( self && ZYPP_IS_CONTEXT(self), false );

  if ( self->_data.godPtr )
    return true; // already loaded

  try
  {
    self->_data.godPtr = zypp::getZYpp();
  }
  catch ( const zypp::ZYppFactoryException & excpt_r )
  {
    ZYPP_CAUGHT( excpt_r );
    ERR <<
      " Could not access the package manager engine."
      " This usually happens when you have another application (like YaST)"
      " using it at the same time. Close the other applications and try again."<<std::endl;
    return false;
  }
  catch ( const zypp::Exception & excpt_r)
  {
    ZYPP_CAUGHT( excpt_r );
    ERR << excpt_r.msg() << std::endl;
    return false;
  }

  zypp::sat::Pool satpool( zypp::sat::Pool::instance() );
  if ( 1 )
  {
    USR << "*** load target '" << zypp::Repository::systemRepoAlias() << "'\t" << std::endl;
    self->_data.godPtr->initializeTarget( self->_data.sysRoot );
    self->_data.godPtr->target()->load();
    USR << satpool.systemRepo() << std::endl;
  }

  self->_data._manager = zyppng::util::share_gobject( zypp_repo_manager_new( self, self->_data.sysRoot ) );
  auto &repoManager = self->_data._manager->_data.zyppRepoMgr;
  zypp::RepoInfoList repos = repoManager.knownRepositories();
  for_( it, repos.begin(), repos.end() )
  {
    zypp::RepoInfo & nrepo( *it );

    if ( ! nrepo.enabled() )
      continue;

    if ( ! repoManager.isCached( nrepo ) )
    {
      USR << zypp::str::form( "*** omit uncached repo '%s' (do 'zypper refresh')", nrepo.name().c_str() ) << std::endl;
      continue;
    }

    USR << zypp::str::form( "*** load repo '%s'\t", nrepo.name().c_str() ) << std::flush;
    try
    {
      repoManager.loadFromCache( nrepo );
      USR << satpool.reposFind( nrepo.alias() ) << std::endl;
    }
    catch ( const zypp::Exception & exp )
    {
      USR << exp.asString() + "\n" + exp.historyAsString() << std::endl;
      USR << zypp::str::form( "*** omit broken repo '%s' (do 'zypper refresh')", nrepo.name().c_str() ) << std::endl;
      continue;
    }
  }

  return true;
}

const gchar * zypp_context_version ( ZyppContext *self )
{
  return self->_data.version.c_str();
}

void zypp_context_set_version ( ZyppContext *self, const gchar * ver )
{
  self->_data.version = std::string(ver);
}

ZyppRepoManager *zypp_context_get_repo_manager ( ZyppContext *self )
{
  return self->_data._manager.get();
}

ZyppDownloader *zypp_context_get_downloader ( ZyppContext *self )
{
  return self->_data._downloader.get();
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
