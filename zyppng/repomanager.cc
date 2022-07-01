/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/repomanager_p.h"
#include "private/repository_p.h"
#include "private/context_p.h"
#include <string>
#include <iostream>

// define the GObject stuff
G_DEFINE_TYPE(ZyppRepoManager, zypp_repo_manager, G_TYPE_OBJECT)

typedef enum
{
  CONSTR_CONTEXT = 1,
  N_PROPERTIES
} ZyppRepoManagerProperty;
static GParamSpec *obj_properties[N_PROPERTIES] = { NULL };

/* Signals */
typedef enum {
    //FIRST_SIG = 1,
    LAST_SIGNAL
} ZyppRepoManagerSignals;
//static guint signals[LAST_SIGNAL] = { 0, };

static void zypp_repo_manager_dispose (GObject *gobject)
{
  /* In dispose(), you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a
   * reference.
   */

  // always call parent class dispose
  G_OBJECT_CLASS (zypp_repo_manager_parent_class)->dispose (gobject);
}

static void zypp_repo_manager_finalize (GObject *gobject)
{
  ZyppRepoManager *ptr = ZYPP_REPOMANAGER(gobject);
  g_clear_weak_pointer( &ptr->_data.context );
  ptr->_data.~Cpp();

  // always call parent class finalizer
  G_OBJECT_CLASS (zypp_repo_manager_parent_class)->finalize (gobject);
}

static void
zypp_repo_manager_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ZyppRepoManager *self = ZYPP_REPOMANAGER (object);
  g_return_if_fail( ZYPP_IS_REPOMANAGER(object) );

  switch ((ZyppRepoManagerProperty)property_id )
    {
      case CONSTR_CONTEXT: {
        ZyppContext *obj = ZYPP_CONTEXT(g_value_get_object( value ));
        g_return_if_fail( ZYPP_IS_CONTEXT(obj) );
        g_set_weak_pointer( &self->_data.context, obj );
        break;
      }
      default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
zypp_repo_manager_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  ZyppRepoManager *self = ZYPP_REPOMANAGER (object);
  g_return_if_fail( ZYPP_IS_REPOMANAGER(object) );

  switch ((ZyppRepoManagerProperty)property_id )
    {
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

void zypp_repo_manager_class_init( ZyppRepoManagerClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = zypp_repo_manager_dispose;
  object_class->finalize = zypp_repo_manager_finalize;

  object_class->set_property = zypp_repo_manager_set_property;
  object_class->get_property = zypp_repo_manager_get_property;

  obj_properties[CONSTR_CONTEXT] =
  g_param_spec_object( "zyppcontext",
                       "ZyppContext",
                       "The zypp context this repo manager belongs to",
                       zypp_context_get_type(),
                       GParamFlags( G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE) );

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     obj_properties);
}

/*
 * Method definitions.
 */
void zypp_repo_manager_init( ZyppRepoManager *ctx )
{
  new (&ctx->_data) ZyppRepoManager::Cpp();
}

ZyppRepoManager *zypp_repo_manager_new( ZyppContext *ctx, const zypp::RepoManagerOptions &options )
{
  auto ret = static_cast<ZyppRepoManager *>(g_object_new( zypp_repo_manager_get_type(), "zyppcontext", ctx, nullptr ));
  ret->_data.zyppRepoMgr = zypp::RepoManager( options );
  return ret;
}

GList *zypp_repo_manager_get_repos ( ZyppRepoManager *self )
{
  GList *ret = nullptr;
  auto ctx = self->_data.context;

  g_return_val_if_fail( ctx != nullptr, nullptr );

  for ( const auto &repo : ctx->_data.godPtr->pool().knownRepositories() ) {
    ret = g_list_append( ret, zypp_repository_new( repo, self ) );
  }

  return ret;
}
