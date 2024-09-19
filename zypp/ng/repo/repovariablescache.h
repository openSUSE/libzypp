/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_REPO_REPOVARIABLES_CACHE_H_INCLUDED
#define ZYPP_NG_REPO_REPOVARIABLES_CACHE_H_INCLUDED

#include <functional>
#include <zypp-core/Pathname.h>
#include <string>
#include <map>

namespace zyppng {
  class ContextBase;
}

namespace zyppng::repo {

  class RepoVarsMap : public std::map<std::string,std::string>
  {
  public:

    ~RepoVarsMap() = default;
    RepoVarsMap(const RepoVarsMap &) = delete;
    RepoVarsMap(RepoVarsMap &&) = delete;
    RepoVarsMap &operator=(const RepoVarsMap &) = delete;
    RepoVarsMap &operator=(RepoVarsMap &&) = delete;

    const std::string *lookup(const std::string &name_r);

    std::ostream &dumpOn(std::ostream &str) const;

  private:
    friend class zyppng::ContextBase; // only context may create a cache

    RepoVarsMap(ContextBase &myContext);
    /** Get first line from file */
    bool parse( const zypp::Pathname &dir_r, const std::string &str_r );

    /** Derive \c releasever_major/_minor from \c releasever, keeping or overwrititing existing values. */
    void deriveFromReleasever(const std::string &stem_r, bool overwrite_r);

    /** Split \c releasever at \c '.' and store major/minor parts as requested. */
    void splitReleaseverTo(const std::string &releasever_r,
                           std::string *major_r, std::string *minor_r) const;

    /** Check for conditions overwriting the (user) defined values. */
    const std::string *checkOverride(const std::string &name_r);

    ContextBase &_myCtx; // the context this Cache is owned by
  };


}



#endif // ZYPP_NG_REPO_REPOVARIABLES_CACHE_H_INCLUDED

