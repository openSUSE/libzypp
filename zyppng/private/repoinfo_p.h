/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPPNG_PRIVATE_REPOINFO_P_H
#define ZYPPNG_PRIVATE_REPOINFO_P_H

#include <zyppng/repoinfo.h>
#include <zypp/RepoInfo.h>

struct _ZyppRepoInfo
{
  GObjectClass            parent_class;
  struct Cpp {
    zypp::RepoInfo _info;
  } _data;
};

ZyppRepoInfo *zypp_repo_info_wrap_cpp ( zypp::RepoInfo &&info );

#endif // ZYPPNG_PRIVATE_REPOINFO_P_H
