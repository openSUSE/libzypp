/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_GLIB_REPO_MANAGER_OPTIONS_P_H
#define ZYPP_GLIB_REPO_MANAGER_OPTIONS_P_H

#include <glib.h>
#include <glib-object.h>
#include <zypp-glib/zypp-glib_export.h>

G_BEGIN_DECLS

#define ZYPP_TYPE_REPO_MANAGER_OPTIONS ( zypp_repo_manager_options_get_type() )

typedef struct _ZyppRepoManagerOptions ZyppRepoManagerOptions;

GType zypp_repo_manager_options_get_type (void) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_repo_manager_options_new: (constructor):
 * @root: (in): The prefix for all paths
 * Returns: (transfer full): newly created #ZyppRepoManagerOptions
 */
ZyppRepoManagerOptions *zypp_repo_manager_options_new( const gchar *root ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_repo_manager_options_copy: (skip):
 *
 * Copy the boxed type
 */
ZyppRepoManagerOptions * zypp_repo_manager_options_copy ( ZyppRepoManagerOptions *self ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_repo_manager_options_free: (skip):
 *
 * Free the boxed type
 */
void zypp_repo_manager_options_free ( ZyppRepoManagerOptions *self ) LIBZYPP_GLIB_EXPORT;


/**
 * zypp_repo_manager_options_get_root:
 *
 * Returns: (transfer full): The currently managed path
 */
gchar *zypp_repo_manager_options_get_root( ZyppRepoManagerOptions *self ) LIBZYPP_GLIB_EXPORT;

G_END_DECLS

#endif // ZYPP_REPO_MANAGER_OPTIONS_P_H
