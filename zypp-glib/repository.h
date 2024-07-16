/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_REPOSITORY_H
#define ZYPP_GLIB_REPOSITORY_H

#include <glib-object.h>
#include <zypp-glib/repoinfo.h>
#include <zypp-glib/zypp-glib_export.h>

G_BEGIN_DECLS

typedef struct _ZyppContext ZyppContext;
typedef struct _ZyppRepoManager ZyppRepoManager;

#define ZYPP_TYPE_REPOSITORY (zypp_repository_get_type ())
#pragma GCC visibility push(default)
G_DECLARE_FINAL_TYPE ( ZyppRepository, zypp_repository, ZYPP, REPOSITORY, GObject )
#pragma GCC visibility pop

/**
 * zypp_repository_get_name:
 *
 * Returns: (transfer full): Name of the repository
 */
gchar *zypp_repository_get_name( ZyppRepository *self ) LIBZYPP_GLIB_EXPORT;


/**
 * zypp_repository_get_repoinfo:
 *
 * Returns: (transfer full): The corresponding ZyppRepoInfo
 */
ZyppRepoInfo *zypp_repository_get_repoinfo( ZyppRepository *self ) LIBZYPP_GLIB_EXPORT;


G_END_DECLS

#ifdef  __cplusplus
#include <zypp-glib/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppRepository, zypp_repository, ZYPP, REPOSITORY )
#endif

#endif // ZYPP_GLIB_REPOSITORY_H
