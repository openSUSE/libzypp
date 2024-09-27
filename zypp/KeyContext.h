#ifndef KEYCONTEXT_H_
#define KEYCONTEXT_H_

#include <zypp/ng/repoinfo.h>
#include <optional>

namespace zypp {

  struct KeyContext
  {
  public:
    KeyContext(){}
    KeyContext( const zyppng::RepoInfo & repoinfo ) : _repoInfo( repoinfo ) {}

    /** Is the context unknown? */
    bool empty() const { return (!_repoInfo.has_value()) || _repoInfo->alias().empty(); }

  public:
    const std::optional<zyppng::RepoInfo> &repoInfo() const { return _repoInfo; }
    void setRepoInfo(const zyppng::RepoInfo & repoinfo) { _repoInfo = repoinfo; }

  private:
    std::optional<zyppng::RepoInfo> _repoInfo;
  };

}

#endif /*KEYCONTEXT_H_*/
