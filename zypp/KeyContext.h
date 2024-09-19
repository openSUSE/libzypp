/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/KeyManager.h
 *
*/
#ifndef KEYCONTEXT_H_
#define KEYCONTEXT_H_

#include <optional>
#include <zypp/Globals.h>
#include <zypp-core/base/PtrTypes.h>

namespace zyppng {
  class RepoInfo;
}

namespace zypp {

  class RepoInfo;

  struct ZYPP_API KeyContext
  {
    class Impl;
  public:
    KeyContext();
    KeyContext(const zyppng::RepoInfo &repoinfo);
    KeyContext(const KeyContext &) = default;
    KeyContext(KeyContext &&) = default;
    KeyContext &operator=(const KeyContext &) = default;
    KeyContext &operator=(KeyContext &&) = default;
    ~KeyContext() = default;

    /** Is the context unknown? */
    bool empty() const;

  public:
#ifdef __cpp_lib_optional // YAST/PK explicitly use c++11 until 15-SP3
    const std::optional<zyppng::RepoInfo> &ngRepoInfo() const;
#endif
    void setRepoInfo(const zyppng::RepoInfo &repoinfo);

    ZYPP_BEGIN_LEGACY_API
    KeyContext(const RepoInfo &repoinfo) ZYPP_INTERNAL_DEPRECATE;
    const RepoInfo repoInfo() const ZYPP_INTERNAL_DEPRECATE;
    void setRepoInfo(const RepoInfo &repoinfo) ZYPP_INTERNAL_DEPRECATE;
    ZYPP_END_LEGACY_API
  private:
    zypp::RWCOW_pointer<Impl> _pimpl;
  };

}

#endif /*KEYCONTEXT_H_*/
