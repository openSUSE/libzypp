/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_PRIVATE_REPO_MANAGER_OPTIONS_P_H
#define ZYPP_GLIB_PRIVATE_REPO_MANAGER_OPTIONS_P_H

#include <zypp-glib/repomanageroptions.h>
#include <zypp/RepoManagerOptions.h>

struct _ZyppRepoManagerOptions {
  zypp::RepoManagerOptions _opts;
};

ZyppRepoManagerOptions *zypp_repo_manager_options_new( const zypp::RepoManagerOptions &rO );

#endif // ZYPP_GLIB_PRIVATE_REPO_MANAGER_OPTIONS_P_H
