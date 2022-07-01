/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPPNG_PRIVATE_REPOMANAGER_P_H
#define ZYPPNG_PRIVATE_REPOMANAGER_P_H

#include <zyppng/repomanager.h>
#include <zypp/RepoManager.h>

struct _ZyppRepoManager
{
  GObjectClass            parent_class;
  struct Cpp {
    ZyppContext *context  = nullptr;
    zypp::RepoManager zyppRepoMgr;
  } _data;
};

ZyppRepoManager *zypp_repo_manager_new( ZyppContext *ctx, const zypp::RepoManagerOptions &options = zypp::RepoManagerOptions() );


#endif // ZYPPNG_PRIVATE_REPOMANAGER_P_H
