/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPPNG_PRIVATE_REPOSITORY_P_H
#define ZYPPNG_PRIVATE_REPOSITORY_P_H

#include <zyppng/repository.h>
#include <zypp/Repository.h>

ZyppRepository *zypp_repository_new( const zypp::Repository &cppVersion, ZyppRepoManager *mgr );

#endif // ZYPPNG_PRIVATE_REPOSITORY_P_H
