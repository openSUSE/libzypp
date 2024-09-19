/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#include "repovariablescache.h"

#include <fstream>

#include <zypp/ng/context.h>
#include <zypp/Target.h>

namespace zyppng::repo {

  namespace env
  {
    /** Use faked releasever (e.g. for 'zupper dup' to next distro version */
    inline std::string ZYPP_REPO_RELEASEVER()
    {
      const char * env = getenv("ZYPP_REPO_RELEASEVER");
      return( env ? env : "" );
    }
  }

  RepoVarsMap::RepoVarsMap(ContextBase &myContext) : _myCtx(myContext)
  { }

  const std::string *RepoVarsMap::lookup(const std::string &name_r) {
    if (empty()) // at init / after reset
    {
      // load user definitions from vars.d

      // @TODO-NG check if  _myCtx.contextRoot() is correct, before this used the config() repomanager root
      zypp::filesystem::dirForEach( _myCtx.contextRoot() /
                             _myCtx.config().varsPath(),
                             zypp::filesystem::matchNoDots(),
                             std::bind(&RepoVarsMap::parse, this, std::placeholders::_1, std::placeholders::_2));
      // releasever_major/_minor are per default derived from releasever.
      // If releasever is userdefined, inject missing _major/_minor too.
      deriveFromReleasever("releasever",
                           /*dont't overwrite user defined values*/ false);

      dumpOn(DBG);
      // add builtin vars except for releasever{,_major,_minor} (see
      // checkOverride)
      {
        const zypp::Arch &arch( _myCtx.config().systemArchitecture() );
        {
          std::string &var(operator[]("arch"));
              if (var.empty())
              var = arch.asString();
        }
        {
          std::string &var(operator[]("basearch"));
              if (var.empty())
              var = arch.baseArch().asString();
        }
      }
    }

    const std::string *ret = checkOverride(name_r);
    if (!ret) {
      // get value from map
      iterator it = find(name_r);
      if (it != end())
        ret = &(it->second);
    }

    return ret;
  }
  std::ostream &RepoVarsMap::dumpOn(std::ostream &str) const {
    for (auto &&kv : *this) {
      str << '{' << kv.first << '=' << kv.second << '}' << std::endl;
    }
    return str;
  }
  bool RepoVarsMap::parse(const zypp::Pathname &dir_r, const std::string &str_r) {
    std::ifstream file((dir_r / str_r).c_str());
    operator[](str_r) = zypp::str::getline(file, /*trim*/ false);
    return true;
  }
  void RepoVarsMap::deriveFromReleasever(const std::string &stem_r,
                                         bool overwrite_r) {
    if (count(stem_r)) // releasever is defined..
    {
      const std::string &stem_major(stem_r + "_major");
      const std::string &stem_minor(stem_r + "_minor");
      if (overwrite_r)
        splitReleaseverTo(operator[](stem_r), &operator[](stem_major),
            &operator[](stem_minor));
      else
        splitReleaseverTo(operator[](stem_r),
            count(stem_major) ? nullptr : &operator[](stem_major),
            count(stem_minor) ? nullptr
                              : &operator[](stem_minor));
    }
  }
  void RepoVarsMap::splitReleaseverTo(const std::string &releasever_r,
                                      std::string *major_r,
                                      std::string *minor_r) const {
    if (major_r || minor_r) {
      std::string::size_type pos = releasever_r.find('.');
      if (pos == std::string::npos) {
        if (major_r)
          *major_r = releasever_r;
        if (minor_r)
          minor_r->clear();
      } else {
        if (major_r)
          *major_r = releasever_r.substr(0, pos);
        if (minor_r)
          *minor_r = releasever_r.substr(pos + 1);
      }
    }
  }
  const std::string *RepoVarsMap::checkOverride(const std::string &name_r) {
    ///////////////////////////////////////////////////////////////////
    // Always check for changing releasever{,_major,_minor} (bnc#943563)
    if (zypp::str::startsWith(name_r, "releasever") &&
        (name_r.size() == 10 || strcmp(name_r.c_str() + 10, "_minor") == 0 ||
         strcmp(name_r.c_str() + 10, "_major") == 0)) {
      std::string val(env::ZYPP_REPO_RELEASEVER());
      if (!val.empty()) {
        // $ZYPP_REPO_RELEASEVER always overwrites any defined value
        if (val != operator[]("$releasever")) {
          operator[]("$releasever") = std::move(val);
          deriveFromReleasever("$releasever",
                               /*overwrite previous values*/ true);
        }
        return &operator[]("$" + name_r);
      } else if (!count(name_r)) {
        // No user defined value, so we follow the target
        zypp::Target_Ptr trg(_myCtx.target());
        if (trg)
          val = trg->distributionVersion();
        else
          val = zypp::Target::distributionVersion( _myCtx.contextRoot() );

        if (val != operator[]("$_releasever")) {
          operator[]("$_releasever") = std::move(val);
          deriveFromReleasever("$_releasever",
                               /*overwrite previous values*/ true);
        }
        return &operator[]("$_" + name_r);
      }
      // else:
      return nullptr; // get user value from map
    }
    ///////////////////////////////////////////////////////////////////

    return nullptr; // get user value from map
  }
} // namespace zyppng::repo
