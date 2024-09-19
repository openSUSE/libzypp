/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
#ifndef ZYPP_NG_REPO_REPOVARIABLES_H_INCLUDED
#define ZYPP_NG_REPO_REPOVARIABLES_H_INCLUDED

#include <zypp-core/Url.h>
#include <zypp/base/ValueTransform.h>
#include <string>
#include <zypp/repo/RepoVariables.h>

namespace zyppng {

  namespace repo  {

    /*
     * Since we need the RepoVar replacer as part of the public zypp API
     * ( because of RepoInfo returning it as part of a iterator type ) they are
     * defined in zypp::repo and only pulled into the zyppng namespace via using declarations.
     * Once we can drop legacy APIs, or find a better way, this can be changed.
     */
    using RepoVarRetrieverFunctor = zypp::repo::RepoVarRetrieverFunctor;

    template <typename T, auto memFn = &T::resolveRepoVar >
    using RepoVarRetriever = zypp::repo::RepoVarRetriever<T, memFn>;

    using RepoVariablesStringReplacer = zypp::repo::RepoVariablesStringReplacerNg;
    using RepoVariablesUrlReplacer = zypp::repo::RepoVariablesUrlReplacerNg;

} // namespace repo

/** \relates RepoVariablesStringReplacer Helper managing repo variables replaced strings */
using RepoVariablesReplacedString = zypp::base::ValueTransform<std::string, repo::RepoVariablesStringReplacer>;

/** \relates RepoVariablesStringReplacer Helper managing repo variables replaced string lists */
using RepoVariablesReplacedStringList = zypp::base::ContainerTransform<std::list<std::string>, repo::RepoVariablesStringReplacer>;

/** \relates RepoVariablesUrlReplacer Helper managing repo variables replaced urls */
using RepoVariablesReplacedUrl = zypp::base::ValueTransform<zypp::Url, repo::RepoVariablesUrlReplacer>;

/** \relates RepoVariablesUrlReplacer Helper managing repo variables replaced url lists */
using RepoVariablesReplacedUrlList = zypp::base::ContainerTransform<std::list<zypp::Url>, repo::RepoVariablesUrlReplacer>;



}

#endif //ZYPP_NG_REPO_REPOVARIABLES_H_INCLUDED
