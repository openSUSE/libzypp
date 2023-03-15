/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPPNG_REPOSITORY_H
#define ZYPPNG_REPOSITORY_H

#include <glib-object.h>
#include <zyppng/repoinfo.h>

G_BEGIN_DECLS

typedef struct _ZyppContext ZyppContext;
typedef struct _ZyppRepoManager ZyppRepoManager;

#define ZYPP_TYPE_REPOSITORY (zypp_repository_get_type ())
G_DECLARE_FINAL_TYPE ( ZyppRepository, zypp_repository, ZYPP, REPOSITORY, GObject )

/**
 * zypp_repository_get_name:
 *
 * Returns: (transfer full): Name of the repository
 */
gchar *zypp_repository_get_name( ZyppRepository *self );


/**
 * zypp_repository_get_repoinfo:
 *
 * Returns: (transfer full): The corresponding ZyppRepoInfo
 */
ZyppRepoInfo *zypp_repository_get_repoinfo( ZyppRepository *self );


G_END_DECLS

#ifdef  __cplusplus
#include <zyppng/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppRepository, zypp_repository, ZYPP, REPOSITORY )
#endif

#endif // ZYPPNG_REPOSITORY_H
