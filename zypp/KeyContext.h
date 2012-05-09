#ifndef KEYCONTEXT_H_
#define KEYCONTEXT_H_

#include "zypp/base/Macros.h"
#include "zypp/RepoInfo.h"

namespace zypp {

  struct ZYPP_API KeyContext
  {
  public:
    /** Is the context unknown? */
    bool empty() const { return _repoInfo.alias().empty(); }
    
  public:
    const RepoInfo repoInfo() const { return _repoInfo; }
    void setRepoInfo(const RepoInfo & repoinfo) { _repoInfo = repoinfo; }
  
  private:
    RepoInfo _repoInfo;
  };

}

#endif /*KEYCONTEXT_H_*/
