/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#include "private/repomanager_p.h"
#include "private/serviceinfo_p.h"
#include "private/repoinfo_p.h"
#include "private/error_p.h"
#include "expected.h"
#include <zypp-core/zyppng/ui/progressobserver.h>
#include <zypp-core/zyppng/pipelines/asyncresult.h>

#include "utils/GList"
#include "private/cancellable_p.h"
#include "progressobserver.h"
#include <zypp-glib/zyppenums.h>

// define the GObject stuff
G_DEFINE_TYPE_WITH_PRIVATE(ZyppRepoManager, zypp_repo_manager, G_TYPE_OBJECT)
G_DEFINE_QUARK (zypp-repo-manager-error-quark, zypp_repo_manager_error)

typedef enum
{
  PROP_CPPOBJ = 1,
  CONTEXT_PROPERTY,
  N_PROPERTIES
} ZyppRepoManagerProperty;
static GParamSpec *obj_properties[N_PROPERTIES] = { NULL };

/* Signals */
typedef enum {
    //FIRST_SIG = 1,
    LAST_SIGNAL
} ZyppRepoManagerSignals;
//static guint signals[LAST_SIGNAL] = { 0, };

#define ZYPP_REPO_MANAGER_D() \
  ZYPP_GLIB_WRAPPER_D( ZyppRepoManager, zypp_repo_manager )

ZYPP_DECLARE_GOBJECT_BOILERPLATE ( ZyppRepoManager, zypp_repo_manager )
ZYPP_DEFINE_GOBJECT_BOILERPLATE  ( ZyppRepoManager, zypp_repo_manager, ZYPP, REPOMANAGER )

void zypp_repo_manager_class_init( ZyppRepoManagerClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  ZYPP_INIT_GOBJECT_BOILERPLATE_KLASS ( zypp_repo_manager, object_class );

  obj_properties[PROP_CPPOBJ] = ZYPP_GLIB_ADD_CPPOBJ_PROP();

  obj_properties[CONTEXT_PROPERTY] =
  g_param_spec_object( "zyppcontext",
                       "ZyppContext",
                         "The zypp context this repo manager belongs to",
                       zypp_context_get_type(),
                       GParamFlags( G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE) );

  /*
   for now we do not support passing RepoManagerOptions, the context defines those
  obj_properties[OPTIONS_PROPERTY] =
  g_param_spec_boxed( "options",
                       "ZyppRepoManagerOptions",
                       "The repo manager options to be used",
                       zypp_repo_manager_options_get_type (),
                       GParamFlags( G_PARAM_CONSTRUCT | G_PARAM_READWRITE ) );
  */

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     obj_properties);
}

void ZyppRepoManagerPrivate::initializeCpp( )
{
  g_return_if_fail ( _constrProps.has_value() );

  if ( _constrProps->_cppObj ) {
    _cppObj = std::move( _constrProps->_cppObj );
  } else {
    if ( !_constrProps->_ctx ) g_error("Context argument can not be NULL");
    auto ctx = zypp_context_get_cpp( _constrProps->_ctx.get() );
    _cppObj = zyppng::AsyncRepoManager::create ( ctx, zyppng::RepoManagerOptions(ctx) );
  }
  _constrProps.reset();
}

static void
zypp_repo_manager_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  g_return_if_fail( ZYPP_IS_REPOMANAGER(object) );
  ZyppRepoManager *self = ZYPP_REPOMANAGER (object);

  ZYPP_REPO_MANAGER_D();

  switch ((ZyppRepoManagerProperty)property_id )
    {
      case PROP_CPPOBJ:
        g_return_if_fail( d->_constrProps ); // only if the constr props are still valid
        ZYPP_GLIB_SET_CPPOBJ_PROP( zyppng::AsyncRepoManagerRef, value, d->_constrProps->_cppObj )

      case CONTEXT_PROPERTY: {
        ZyppContext *obj = ZYPP_CONTEXT(g_value_get_object( value ));
        g_return_if_fail( ZYPP_IS_CONTEXT(obj) );
        if ( d->_constrProps ) {
          d->_constrProps->_ctx = zypp::glib::ZyppContextRef( obj, zypp::glib::retain_object );
        }
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
  g_return_if_fail( ZYPP_IS_REPOMANAGER(object) );
//  ZyppRepoManager *self = ZYPP_REPOMANAGER (object);
//  ZYPP_REPO_MANAGER_D();

  switch ((ZyppRepoManagerProperty)property_id ) {
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

/*
 * Method definitions.
 */

ZyppRepoManager *zypp_repo_manager_new( ZyppContext *ctx )
{
  g_return_val_if_fail( ctx != nullptr, nullptr );
  return static_cast<ZyppRepoManager *>( g_object_new( zypp_repo_manager_get_type(), "zyppcontext", ctx, nullptr ) );
}

ZyppRepoManager *zypp_repo_manager_new_initialized( ZyppContext *ctx, GError **error )
{
  auto rM = zypp::glib::ZyppRepoManagerRef( zypp_repo_manager_new(ctx), zypp::glib::retain_object );
  if ( !zypp_repo_manager_initialize( rM.get(), error ) )
    return nullptr;

  return rM.detach ();
}


gboolean zypp_repo_manager_initialize( ZyppRepoManager *self, GError **error )
{
  ZYPP_REPO_MANAGER_D();

  using namespace zyppng::operators;
  try {
    d->cppType ()->initialize().unwrap();
    return true;
  } catch ( ... ) {
    zypp_error_from_exception ( error, std::current_exception() );
  }
  return false;
}

#if 0
GList *zypp_repo_manager_get_repos ( ZyppRepoManager *self )
{
  ZYPP_REPO_MANAGER_D();
  GList *ret = nullptr;

  auto ctx = d->_cppObj->zyppContext();

  g_return_val_if_fail( ctx.operator bool() , nullptr );

  for ( const auto &repo : ctx->pool().knownRepositories() ) {
    ret = g_list_append( ret, zypp_repository_new( repo, self ) );
  }

  return ret;
}
#endif

GList *zypp_repo_manager_get_known_repos(ZyppRepoManager *self)
{
  ZYPP_REPO_MANAGER_D();

  GList *ret = nullptr;

  g_return_val_if_fail( d->_cppObj.operator bool() , nullptr );

  for ( auto i = d->_cppObj->repoBegin (); i != d->_cppObj->repoEnd(); i++ ) {
    ret = g_list_append( ret, zypp_wrap_cpp ( i->second ) );
  }

  return ret;
}

GList *zypp_repo_manager_get_known_services(ZyppRepoManager *self)
{
  ZYPP_REPO_MANAGER_D();

  GList *ret = nullptr;

  g_return_val_if_fail( d->_cppObj.operator bool() , nullptr );

  for ( auto i = d->_cppObj->serviceBegin (); i != d->_cppObj->serviceEnd(); i++ ) {
    ret = g_list_append( ret, zypp_wrap_cpp ( i->second ) );
  }

  return ret;
}


struct TaskData {
  zyppng::AsyncOpBaseRef _op;
  zyppng::ProgressObserverRef _prog;
};

void zypp_repo_manager_refresh_repo ( ZyppRepoManager *self, ZyppRepoInfo *repo, gboolean forceDownload, GCancellable *cancellable, GAsyncReadyCallback cb, gpointer user_data )
{
  ZYPP_REPO_MANAGER_D();

  auto ctx = d->_cppObj->zyppContext();
  zyppng::ProgressObserverRef progress = zyppng::ProgressObserver::makeSubTask( ctx->progressObserver(), 1.0, "Refreshing Repo und so" );

  g_return_if_fail( zyppng::EventDispatcher::instance()->glibContext() != nullptr );

  if ( !repo ) {
    GError *err = nullptr;
    zypp_error_from_exception (&err, ZYPP_EXCPT_PTR ( zypp::Exception("Invalid arguments to zypp_repo_manager_refresh_repo_async") ));
    g_task_report_error( self, cb, user_data, (gpointer)zypp_repo_manager_refresh_repo, err );
    return;
  }

  g_main_context_push_thread_default( zyppng::EventDispatcher::instance()->glibContext() );
  zypp::glib::GTaskRef task = zypp::glib::adopt_gobject( g_task_new( self, cancellable, cb, user_data ) );
  g_main_context_pop_thread_default ( zyppng::EventDispatcher::instance()->glibContext() );


  auto op = d->_cppObj->refreshMetadata( zypp_repo_info_get_cpp(repo), forceDownload ?  zypp::RepoManagerFlags::RefreshForced : zypp::RepoManagerFlags::RefreshIfNeeded, progress );

  auto taskData = std::make_unique<TaskData>();
  taskData->_prog = std::move(progress);
  taskData->_op   = op;

  // task keeps the reference to our async op
  g_task_set_task_data( task.get(), new zyppng::AsyncOpBaseRef(op), zypp::glib::g_destroy_notify_delete<zyppng::AsyncOpBaseRef> );

  auto taskPtr = task.get();

  // we keep the reference to our task
  d->_asyncOpsRunning.push_back( std::move(task) );

  // this will get triggered immediately if the task is already done
  op->onReady(
    [ task = taskPtr /*do not take a reference*/ ]( zyppng::expected<zyppng::RepoInfo> res ){
          MIL << "Async Operation finished" << std::endl;
          if ( !res ) {
            GError *err = nullptr;
            zypp_error_from_exception (&err, res.error() );
            g_task_return_error( task, err );
            return;
          }

          // return the RepoInfo to the caller via callback
          g_task_return_pointer( task, zypp_wrap_cpp( res.get() ), g_object_unref );
        }
  );
}

ZyppRepoInfo *zypp_repo_manager_refresh_repo_finish ( ZyppRepoManager *self,
                                                      GAsyncResult  *result,
                                                      GError **error )
{
  MIL << "Inside finish func" << std::endl;
  g_return_val_if_fail (g_task_is_valid (result, self), NULL);

  MIL << "Inside finish func 2" << std::endl;
  // if the result is tagged with the zypp_repo_manager_refresh_repo func it was created before the task
  // was even started and registered. No need to clean it
  if ( !g_async_result_is_tagged ( result, (gpointer)zypp_repo_manager_refresh_repo ) ) {
    ZYPP_REPO_MANAGER_D();
    auto i = std::find_if( d->_asyncOpsRunning.begin(), d->_asyncOpsRunning.end(),[&]( const auto &e ) { return e.get() == G_TASK(result); } );
    if ( i != d->_asyncOpsRunning.end() ) {
      d->_asyncOpsRunning.erase(i);
    } else {
      WAR << "Task was not in the list of currently running tasks, thats a bug" << std::endl;
    }
  }

  MIL << "Inside finish func 3" << std::endl;
  return reinterpret_cast<ZyppRepoInfo *>(g_task_propagate_pointer (G_TASK (result), error));
}

