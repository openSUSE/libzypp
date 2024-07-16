/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_REPOINFO_H
#define ZYPP_GLIB_REPOINFO_H

#include <glib-object.h>
#include <zypp-glib/infobase.h>
#include <zypp-glib/zypp-glib_export.h>

G_BEGIN_DECLS

typedef struct _ZyppContext  ZyppContext;
typedef struct _ZyppRepoInfo ZyppRepoInfo;

typedef enum
{
  ZYPP_REPO_NONE,
  ZYPP_REPO_RPMMD,
  ZYPP_REPO_YAST2,
  ZYPP_REPO_RPMPLAINDIR
} ZyppRepoInfoType;

#define ZYPP_TYPE_REPOINFO (zypp_repo_info_get_type ())
#pragma GCC visibility push(default)
G_DECLARE_FINAL_TYPE ( ZyppRepoInfo, zypp_repo_info, ZYPP, REPOINFO, GObject )
#pragma GCC visibility pop

ZyppRepoInfo *zypp_repo_info_new ( ZyppContext *context ) LIBZYPP_GLIB_EXPORT;

/**
 * zypp_repo_info_get_repo_type:
 *
 * Returns: The type of repository
 */
ZyppRepoInfoType zypp_repo_info_get_repo_type( ZyppRepoInfo *self ) LIBZYPP_GLIB_EXPORT;


G_END_DECLS

#ifdef  __cplusplus
#include <zypp-glib/utils/GObjectMemory>
ZYPP_DEFINE_GOBJECT_SIMPLE( ZyppRepoInfo, zypp_repo_info, ZYPP, REPOINFO )
#endif


#endif // ZYPP_GLIB_REPOINFO_H
