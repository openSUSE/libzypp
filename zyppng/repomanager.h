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

#define ZYPP_TYPE_REPOMANAGER (zypp_repo_manager_get_type ())
G_DECLARE_FINAL_TYPE ( ZyppRepoManager, zypp_repo_manager, ZYPP, REPOMANAGER, GObject )

/**
 * zypp_repo_manager_get_repos:
 *
 * Returns: (element-type ZyppRepository) (transfer full): list of constants,
 *          free the list with g_list_free and the elements with gobject_unref when done.
 */
GList *zypp_repo_manager_get_repos ( ZyppRepoManager *self );

G_END_DECLS

#endif // ZYPPNG_REPOMANAGER_H
