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
#include "private/repository_p.h"
#include "zyppng/expected.h"
#include <zyppng/utils/GList>
#include <string>
#include <iostream>
#include <zyppng/zyppenums.h>
#include <zyppng/progressobserver.h>
#include <zyppng/utils/GList>

// define the GObject stuff
G_DEFINE_TYPE(ZyppRepoManager, zypp_repo_manager, G_TYPE_OBJECT)
G_DEFINE_QUARK (zypp-repo-manager-error-quark, zypp_repo_manager_error)

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
  g_return_if_fail( ZYPP_IS_REPOMANAGER(object) );
  ZyppRepoManager *self = ZYPP_REPOMANAGER (object);

  switch ((ZyppRepoManagerProperty)property_id )
    {
      case CONSTR_CONTEXT: {
        ZyppContext *obj = ZYPP_CONTEXT(g_value_get_object( value ));
        g_return_if_fail( ZYPP_IS_CONTEXT(obj) );
        self->_data.context.reset( obj );
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
  //ZyppRepoManager *self = ZYPP_REPOMANAGER (object);
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
  auto ctx = self->_data.context.lock();

  g_return_val_if_fail( ctx.operator bool() , nullptr );

  for ( const auto &repo : ctx->_data.godPtr->pool().knownRepositories() ) {
    ret = g_list_append( ret, zypp_repository_new( repo, self ) );
  }

  return ret;
}

void zypp_repo_manager_test_list ( ZyppRepoManager *self, GList *data )
{
  if ( !data )
    std::cout << "Nothing in the list" << std::endl;

  ZYPP_IS_REPOMANAGER( self );

  int count = 0;
  for ( auto d = data; d != NULL && count < 12; d = d->next ) {
    std::cout << "Data says: " << d->data << std::endl;
    std::cout << "Next says: " << d->next << std::endl;
    count++;
  }

  zyppng::GListContainer<gsize> cont( data, zyppng::GListContainer<gsize>::Ownership::None );
  std::cout << "Container size: " << cont.size () << std::endl;

#if 0
  for ( auto i = cont.begin(); i != cont.end(); i++ ) {
    std::cout << "Iterator 1 says: " << *i << std::endl;
  }
#endif

  cont.push_back(42);

#if 0
  for ( const auto &i : cont ) {
    std::cout << "Iterator 2 says: " << i << std::endl;
  }
#endif

}

GList *zypp_repo_manager_get_known_repos(ZyppRepoManager *self)
{
  GList *ret = nullptr;
  auto ctx = self->_data.context.lock();

  g_return_val_if_fail( ctx.operator bool() , nullptr );

  for ( auto repo : self->_data.zyppRepoMgr.knownRepositories() ) {
    ret = g_list_append( ret, zypp_repo_info_wrap_cpp ( std::move(repo) ) );
  }

  return ret;
}

GList *zypp_repo_manager_get_known_services(ZyppRepoManager *self)
{
  GList *ret = nullptr;
  auto ctx = self->_data.context.lock();

  g_return_val_if_fail( ctx.operator bool() , nullptr );

  for ( const auto &service : self->_data.zyppRepoMgr.knownServices() ) {
    ret = g_list_append( ret, zypp_service_info_wrap_cpp ( zypp::ServiceInfo(service) ) );
  }

  return ret;
}

GList *zypp_repo_manager_refresh_repos( ZyppRepoManager *self, GList *repos, gboolean forceDownload, ZyppProgressObserver *statusTracker )
{
  zyppng::GListContainer<ZyppExpected> results;
  zyppng::GListView<ZyppRepoInfo> repoInfos( repos );

  //keep a reference to the tracker
  zyppng::ZyppProgressObserverRef tracker( statusTracker, zyppng::retain_object );
  zypp_progress_observer_set_current ( statusTracker, 0 );
  zypp_progress_observer_set_base_steps ( statusTracker, repoInfos.size());

  std::vector<zyppng::ZyppProgressObserverRef> subTasksTracker;
  for ( const auto &info : repoInfos ) {
    const auto &zInfo = info->_data._info;
    const std::string &name =  zInfo.name ();
    zyppng::ZyppProgressObserverRef subTask = zyppng::util::g_object_create<ZyppProgressObserver>( "label", name.c_str (), "base-steps", 10 );
    zypp_progress_observer_add_subtask ( statusTracker, subTask.get(), 1.0 );
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

  zypp_progress_observer_set_current ( statusTracker, repoInfos.size() );

  GValue val = G_VALUE_INIT;
  g_value_init( &val, ZYPP_TYPE_REPO_REFRESH_RESULT);
  g_value_set_enum ( &val, ZYPP_REPO_MANAGER_REFRESHED );

  results.push_back ( zypp_expected_new_value (&val) );
  results.push_back ( zypp_expected_new_error ( g_error_new( ZYPP_REPO_MANAGER_ERROR, ZYPP_REPO_MANAGER_ERROR_REF_FAILED, "Refresh failed horribly") ) );
  results.push_back ( zypp_expected_new_value (&val) );
  results.push_back ( zypp_expected_new_error ( g_error_new( ZYPP_REPO_MANAGER_ERROR, ZYPP_REPO_MANAGER_ERROR_REF_FAILED, "Refresh failed horribly") ) );
  return results.take();
}


