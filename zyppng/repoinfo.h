/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPPNG_REPOINFO_H
#define ZYPPNG_REPOINFO_H

#include <glib-object.h>
#include <zyppng/infobase.h>

G_BEGIN_DECLS

typedef struct _ZyppRepoInfo ZyppRepoInfo;

typedef enum
{
  ZYPP_REPO_NONE,
  ZYPP_REPO_RPMMD,
  ZYPP_REPO_YAST2,
  ZYPP_REPO_RPMPLAINDIR
} ZyppRepoInfoType;

#define ZYPP_TYPE_REPOINFO (zypp_repo_info_get_type ())
G_DECLARE_FINAL_TYPE ( ZyppRepoInfo, zypp_repo_info, ZYPP, REPOINFO, GObject )

ZyppRepoInfo *zypp_repo_info_new ( );

/**
 * zypp_repo_info_get_repo_type:
 *
 * Returns: The type of repository
 */
ZyppRepoInfoType zypp_repo_info_get_repo_type( ZyppRepoInfo *self );


G_END_DECLS

#ifdef  __cplusplus
#include <zyppng/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppRepoInfo, zypp_repo_info, ZYPP, REPOINFO )
#endif


#endif // ZYPPNG_REPOINFO_H
