/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_GLIB_PRIVATE_REPOSITORY_P_H
#define ZYPP_GLIB_PRIVATE_REPOSITORY_P_H

#include <zypp-glib/repository.h>
#include <zypp/Repository.h>

ZyppRepository *zypp_repository_new( const zypp::Repository &cppVersion, ZyppRepoManager *mgr );

#endif // ZYPP_GLIB_PRIVATE_REPOSITORY_P_H
