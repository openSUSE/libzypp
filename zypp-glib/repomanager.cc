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
#include "zypp-core/zyppng/ui/progressobserver.h"
#include <zypp-glib/utils/GList>
#include <string>
#include <iostream>
#include <zypp-glib/error.h>
#include <zypp-glib/zyppenums.h>
#include <zypp-glib/progressobserver.h>
#include <zypp-glib/utils/GList>

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

void zypp_repo_manager_refresh_repos_async ( ZyppRepoManager *self, GList *repos, gboolean forceDownload, GCancellable *cancellable, GAsyncReadyCallback cb, gpointer user_data )
{
  ZYPP_REPO_MANAGER_D();

  auto ctx = d->_cppObj->zyppContext();
  auto progress = ctx->progressObserver();

  zypp::glib::GListContainer<ZyppExpected> results;
  zypp::glib::GListView<ZyppRepoInfo> repoInfos( repos );
  std::vector<zyppng::RepoInfo> cppInfos;
  std::transform( repoInfos.begin (), repoInfos.end(), std::back_inserter(cppInfos), []( ZyppRepoInfo *i) { return zypp_repo_info_get_cpp (i); } );

  auto task = [ mgr = d->_cppObj, progress, repos = std::move(cppInfos), forceDownload ]() {
    return mgr->refreshMetadata( repos, forceDownload ?  zypp::RepoManagerFlags::RefreshForced : zypp::RepoManagerFlags::RefreshIfNeeded, progress );
  };

#if 0
  auto cppResults = d->_cppObj->refreshMetadata( std::move(cppInfos), forceDownload ?  zypp::RepoManagerFlags::RefreshForced : zypp::RepoManagerFlags::RefreshIfNeeded, progress );

  // need to give the results back to the calling code
  for ( const auto &res : cppResults ) {
    if ( res.second ) {
      // need to sync back the result
      for ( auto info : repoInfos ) {
        zyppng::RepoInfo &repo = zypp_repo_info_get_cpp (info);
        if ( repo.alias() == res.first.alias() ) {
          repo = res.first;
        }
      }

      GValue val = G_VALUE_INIT;
      g_value_init( &val, ZYPP_TYPE_REPO_REFRESH_RESULT);
      g_value_set_enum ( &val, ZYPP_REPO_MANAGER_REFRESHED );
      results.push_back ( zypp_expected_new_value (&val) );

    } else {
      GError *err = nullptr;
      zypp_error_from_exception ( &err, res.second.error() );
      results.push_back ( zypp_expected_new_error ( err ) );
    }
  }

  //keep a reference to the tracker
  zypp::glib::ZyppProgressObserverRef statusTracker = zypp::glib::zypp_wrap_cpp<ZyppProgressObserver>(progress);
  zypp_progress_observer_set_current ( statusTracker.get(), 0 );
  zypp_progress_observer_set_base_steps ( statusTracker.get(), repoInfos.size());

  std::vector<zypp::glib::ZyppProgressObserverRef> subTasksTracker;
  for ( const auto &info : repoInfos ) {
    const auto &zInfo =  zypp_repo_info_get_cpp(info);
    const std::string &name =  zInfo.name ();
    zypp::glib::ZyppProgressObserverRef subTask = zypp::glib::g_object_create<ZyppProgressObserver>( "label", name.c_str (), "base-steps", 10 );
    zypp_progress_observer_add_subtask ( statusTracker.get(), subTask.get(), 1.0 );
    subTasksTracker.push_back ( subTask );
  }

  std::cerr << "------------------------ STARTING TO GENERATE ----------------------------" << std::endl;

  for ( int i = 0; i < 10; i++ ) {
    for ( auto &t : subTasksTracker ) {
      zypp_progress_observer_inc( t.get(), 1 );
    }
  }
  for ( auto &t : subTasksTracker ) {
    zypp_progress_observer_set_finished ( t.get() );
  }

  zypp_progress_observer_set_current ( statusTracker.get(), repoInfos.size() );

  GValue val = G_VALUE_INIT;
  g_value_init( &val, ZYPP_TYPE_REPO_REFRESH_RESULT);
  g_value_set_enum ( &val, ZYPP_REPO_MANAGER_REFRESHED );

  results.push_back ( zypp_expected_new_value (&val) );
  results.push_back ( zypp_expected_new_error ( g_error_new( ZYPP_REPO_MANAGER_ERROR, ZYPP_REPO_MANAGER_ERROR_REF_FAILED, "Refresh failed horribly") ) );
  results.push_back ( zypp_expected_new_value (&val) );
  results.push_back ( zypp_expected_new_error ( g_error_new( ZYPP_REPO_MANAGER_ERROR, ZYPP_REPO_MANAGER_ERROR_REF_FAILED, "Refresh failed horribly") ) );
  return results.take();
#endif
}

