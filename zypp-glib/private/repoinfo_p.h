/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_GLIB_PRIVATE_REPOINFO_P_H
#define ZYPP_GLIB_PRIVATE_REPOINFO_P_H

#include <zypp-glib/repoinfo.h>
#include <zypp-glib/context.h>
#include <zypp-glib/private/globals_p.h>
#include <zypp/RepoInfo.h>
#include <optional>

struct ZyppRepoInfoPrivate
{
  struct ConstructionProps {
    std::optional<zypp::RepoInfo> _cppObj;
    zypp::glib::ZyppContextRef _context =  nullptr;
  };
  std::optional<ConstructionProps> _constrProps = ConstructionProps();

  zypp::RepoInfo _info{nullptr};

  ZyppRepoInfoPrivate( ZyppRepoInfo *pub ) : _gObject(nullptr) {}
  void initialize();
  void finalize(){}

private:
  ZyppRepoInfo *_gObject = nullptr;
};


struct _ZyppRepoInfo
{
  GObjectClass parent_class;
};

ZyppRepoInfo *zypp_wrap_cpp ( zypp::RepoInfo info );

/**
 * zypp_repo_info_get_cpp: (skip)
 */
zypp::RepoInfo &zypp_repo_info_get_cpp( ZyppRepoInfo *self );

#endif // ZYPP_GLIB_PRIVATE_REPOINFO_P_H
