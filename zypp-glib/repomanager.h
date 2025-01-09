/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_REPOMANAGER_H
#define ZYPP_GLIB_REPOMANAGER_H


#include <glib-object.h>
#include <gio/gio.h>
#include <zypp-glib/zypp-glib_export.h>

G_BEGIN_DECLS

typedef struct _ZyppContext ZyppContext;
typedef struct _ZyppRepoInfo ZyppRepoInfo;
typedef struct _ZyppExpected ZyppExpected;

typedef enum {
  ZYPP_REPO_MANAGER_UP_TO_DATE,
  ZYPP_REPO_MANAGER_REFRESHED
} ZyppRepoRefreshResult;

#define ZYPP_TYPE_REPOMANAGER (zypp_repo_manager_get_type ())

#pragma GCC visibility push(default)
G_DECLARE_FINAL_TYPE ( ZyppRepoManager, zypp_repo_manager, ZYPP, REPOMANAGER, GObject )
#pragma GCC visibility pop

#define ZYPP_REPO_MANAGER_ERROR zypp_repo_manager_error_quark ()
GQuark  zypp_repo_manager_error_quark      (void) LIBZYPP_GLIB_EXPORT;
typedef enum {
  ZYPP_REPO_MANAGER_ERROR_REF_FAILED,
  ZYPP_REPO_MANAGER_ERROR_REF_SKIPPED,
  ZYPP_REPO_MANAGER_ERROR_REF_ABORTED
} ZyppRepoManagerError;

/**
 * zypp_repo_manager_new: (constructor)
 * @ctx: The #ZyppContext the RepoManager should operate on
 *
 * Returns: (transfer full): newly created #ZyppRepoManager
 */
ZyppRepoManager *zypp_repo_manager_new( ZyppContext *ctx ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_repo_manager_new_initialized: (constructor)
 * @ctx: The #ZyppContext the RepoManager should operate on
 *
 * Returns: (transfer full): newly created #ZyppRepoManager
 */
ZyppRepoManager *zypp_repo_manager_new_initialized( ZyppContext *ctx, GError **error ) LIBZYPP_GLIB_EXPORT;


/**
 * zypp_repo_manager_initialize:
 *
 * Loads the known repositories and services.
 *
 * Returns: True if init was successful, otherwise returns false and sets the error
 */
gboolean zypp_repo_manager_initialize( ZyppRepoManager *self, GError **error ) LIBZYPP_GLIB_EXPORT;


/**
 * zypp_repo_manager_get_known_repos:
 *
 * Returns: (element-type ZyppRepoInfo) (transfer full): list of repositories,
 *          free the list with g_list_free and the elements with gobject_unref when done.
 */
GList *zypp_repo_manager_get_known_repos ( ZyppRepoManager *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_repo_manager_get_known_services:
 *
 * Returns: (element-type ZyppServiceInfo) (transfer full): list of existing services,
 *          free the list with g_list_free and the elements with gobject_unref when done.
 */
GList *zypp_repo_manager_get_known_services ( ZyppRepoManager *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_repo_manager_refresh_repo:
 * @self: a #ZyppRepoManager
 * @repo: (transfer none): the repository to refresh
 * @forceDownload: Force downloading the repository even if its up 2 date
 * @cancellable: (nullable)
 * @cb: (scope async) (closure user_data): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 */
void zypp_repo_manager_refresh_repo ( ZyppRepoManager *self, ZyppRepoInfo *repo, gboolean forceDownload, GCancellable *cancellable, GAsyncReadyCallback cb, gpointer user_data ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_repo_manager_refresh_repo_finish:
 * @self: (in): a #ZyppRepoManager
 * @result: (in): where to place the result
 * @error: return location for a GError, or NULL
 *
 * Returns: (transfer full): The refreshed #ZyppRepoInfo object
 */
ZyppRepoInfo *zypp_repo_manager_refresh_repo_finish ( ZyppRepoManager *self, GAsyncResult *result, GError **error ) LIBZYPP_GLIB_EXPORT;


G_END_DECLS

#ifdef  __cplusplus
#include <zypp-glib/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppRepoManager, zypp_repo_manager, ZYPP, REPOMANAGER )
#endif

#endif // ZYPP_GLIB_REPOMANAGER_H
