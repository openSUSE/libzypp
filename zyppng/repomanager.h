/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPPNG_REPOMANAGER_H
#define ZYPPNG_REPOMANAGER_H


#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ZyppContext ZyppContext;
typedef struct _ZyppRepoInfo ZyppRepoInfo;
typedef struct _ZyppExpected ZyppExpected;
typedef struct _ZyppTaskStatus ZyppTaskStatus;

typedef enum {
  ZYPP_REPO_MANAGER_UP_TO_DATE,
  ZYPP_REPO_MANAGER_REFRESHED
} ZyppRepoRefreshResult;

#define ZYPP_TYPE_REPOMANAGER (zypp_repo_manager_get_type ())
G_DECLARE_FINAL_TYPE ( ZyppRepoManager, zypp_repo_manager, ZYPP, REPOMANAGER, GObject )

#define ZYPP_REPO_MANAGER_ERROR zypp_repo_manager_error_quark ()
GQuark  zypp_repo_manager_error_quark      (void);
typedef enum {
  ZYPP_REPO_MANAGER_ERROR_REF_FAILED,
  ZYPP_REPO_MANAGER_ERROR_REF_SKIPPED,
  ZYPP_REPO_MANAGER_ERROR_REF_ABORTED
} ZyppRepoManagerError;


/**
 * zypp_repo_manager_get_repos:
 *
 * Returns: (element-type ZyppRepository) (transfer full): list of repositories,
 *          free the list with g_list_free and the elements with gobject_unref when done.
 */
GList *zypp_repo_manager_get_repos ( ZyppRepoManager *self );

/**
 * zypp_repo_manager_get_known_repos:
 *
 * Returns: (element-type ZyppRepoInfo) (transfer full): list of repositories,
 *          free the list with g_list_free and the elements with gobject_unref when done.
 */
GList *zypp_repo_manager_get_known_repos ( ZyppRepoManager *self );

/**
 * zypp_repo_manager_get_known_services:
 *
 * Returns: (element-type ZyppServiceInfo) (transfer full): list of existing services,
 *          free the list with g_list_free and the elements with gobject_unref when done.
 */
GList *zypp_repo_manager_get_known_services ( ZyppRepoManager *self );


/**
 * zypp_repo_manager_refresh_repos:
 * @self: a #ZyppRepoManager
 * @repos: (element-type ZyppRepoInfo) (transfer none): the repositories to refresh
 * @forceDownload: Force downloading the repository even if its up 2 date
 * @statusTracker: (transfer full) (nullable): Progress tracker
 *
 * Returns: (element-type ZyppExpected) (transfer full): list of results for the refreshed repos
 */
GList *zypp_repo_manager_refresh_repos ( ZyppRepoManager *self, GList *repos, gboolean forceDownload, ZyppTaskStatus *statusTracker );


/**
 * zypp_repo_manager_test_list:
 * @data: (in)(transfer none)(element-type gsize): The path of the file we want to manage
 */
void zypp_repo_manager_test_list ( ZyppRepoManager *self, GList *data );

G_END_DECLS

#ifdef  __cplusplus
#include <zyppng/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppRepoManager, zypp_repo_manager, ZYPP, REPOMANAGER )
#endif

#endif // ZYPPNG_REPOMANAGER_H
