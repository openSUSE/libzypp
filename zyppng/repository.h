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

G_BEGIN_DECLS

typedef struct _ZyppContext ZyppContext;
typedef struct _ZyppRepoManager ZyppRepoManager;

#define ZYPP_TYPE_REPOSITORY (zypp_repository_get_type ())
G_DECLARE_FINAL_TYPE ( ZyppRepository, zypp_repository, ZYPP, REPOSITORY, GObject )

gchar *zypp_repository_get_name( ZyppRepository *self );


G_END_DECLS


#endif // ZYPPNG_REPOSITORY_H
