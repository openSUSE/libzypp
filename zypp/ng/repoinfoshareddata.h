/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/

#ifndef ZYPP_NG_REPOINFOSHAREDDATA_INCLUDED
#define ZYPP_NG_REPOINFOSHAREDDATA_INCLUDED

#include <zypp/repo/GpgCheck.h>
#include <zypp-core/base/defaultintegral.h>
#include <zypp/repo/RepoType.h>
#include <zypp/ng/repo/repoinfobaseshareddata.h>
#include <set>


namespace zyppng {

  /** RepoInfo implementation. */
  struct RepoInfoSharedData : public repo::RepoInfoBaseSharedData
  {
    RepoInfoSharedData( zyppng::ContextBaseRef &&context );
    RepoInfoSharedData(RepoInfoSharedData &&) = delete;
    RepoInfoSharedData &operator=(const RepoInfoSharedData &) = delete;
    RepoInfoSharedData &operator=(RepoInfoSharedData &&) = delete;
    ~RepoInfoSharedData() override {}

  public:
    static const unsigned defaultPriority = 99;
    static const unsigned noPriority = unsigned(-1);

    void setType(const zypp::repo::RepoType &t);

    void setProbedType(const zypp::repo::RepoType &t) const;

    zypp::repo::RepoType type() const;

  public:
    zypp::Pathname licenseTgz(const std::string &name_r) const;

    const RepoVariablesReplacedUrlList &baseUrls() const;

    RepoVariablesReplacedUrlList &baseUrls();

    bool baseurl2dump() const;

    const RepoVariablesReplacedUrlList &gpgKeyUrls() const;

    RepoVariablesReplacedUrlList &gpgKeyUrls();

    const std::set<std::string> &contentKeywords() const;

    void addContent(const std::string &keyword_r);

    bool hasContent() const;

    bool hasContent(const std::string &keyword_r) const;

    /** Signature check result needs to be stored/retrieved from _metadataPath.
     * Don't call them from outside validRepoSignature/setValidRepoSignature
     */
    zypp::TriBool internalValidRepoSignature() const;

    void internalSetValidRepoSignature( zypp::TriBool value_r);

    /** We definitely have a symlink pointing to "zypp::indeterminate" (for repoGpgCheckIsMandatory)?
     * I.e. user accepted the unsigned repo in Downloader. A test whether `internalValidRepoSignature`
     * is zypp::indeterminate would include not yet checked repos, which is unwanted here.
     */
    bool internalUnsignedConfirmed() const;

    bool triBoolFromPath(const zypp::Pathname &path_r, zypp::TriBool &ret_r) const;

    zypp::TriBool triBoolFromPath(const zypp::Pathname &path_r) const;

    zypp::TriBool rawGpgCheck() const			{ return _rawGpgCheck; }
    zypp::TriBool rawRepoGpgCheck() const		{ return _rawRepoGpgCheck; }
    zypp::TriBool rawPkgGpgCheck() const		{ return _rawPkgGpgCheck; }

    void rawGpgCheck( zypp::TriBool val_r )		{ _rawGpgCheck = val_r; }
    void rawRepoGpgCheck( zypp::TriBool val_r )	{ _rawRepoGpgCheck = val_r; }
    void rawPkgGpgCheck( zypp::TriBool val_r )	{ _rawPkgGpgCheck = val_r; }

    bool cfgGpgCheck() const;

    zypp::TriBool cfgRepoGpgCheck() const;

    zypp::TriBool cfgPkgGpgCheck() const;

    void solvPath(zypp::Pathname new_r);

    void metadataPath(zypp::Pathname new_r);

    void packagesPath(zypp::Pathname new_r);

    bool usesAutoMetadataPaths() const;

    zypp::Pathname solvPath() const;

    zypp::Pathname metadataPath() const;

    zypp::Pathname packagesPath() const;

    zypp::DefaultIntegral<unsigned,defaultPriority> priority;

  protected:
    void bindVariables() override;

  private:
    zypp::TriBool _rawGpgCheck;	///< default gpgcheck behavior: Y/N/ZConf
    zypp::TriBool _rawRepoGpgCheck;	///< need to check repo sign.: Y/N/(ZConf(Y/N/gpgCheck))
    zypp::TriBool _rawPkgGpgCheck;	///< need to check pkg sign.: Y/N/(ZConf(Y/N/gpgCheck))
    zypp::TriBool _validRepoSignature; ///< have  signed and valid repo metadata
    zypp::repo::RepoType _type;

  public:
    zypp::TriBool keeppackages;
    RepoVariablesReplacedUrl _mirrorListUrl;
    bool                     _mirrorListForceMetalink;
    zypp::Pathname path;
    std::string service;
    std::string targetDistro;
    mutable bool emptybaseurls;

  private:
    zypp::Pathname _slvPath;
    zypp::Pathname _metadataPath;
    zypp::Pathname _packagesPath;

    mutable RepoVariablesReplacedUrlList _baseUrls;
    mutable std::pair<zypp::FalseBool, std::set<std::string> > _keywords;

    RepoVariablesReplacedUrlList _gpgKeyUrls;

    //  support copying only for clone
    RepoInfoSharedData(const RepoInfoSharedData &) = default;

    friend RepoInfoSharedData * zypp::rwcowClone<RepoInfoSharedData>( const RepoInfoSharedData * rhs );
    /** clone for RWCOW_pointer */
    RepoInfoSharedData *clone() const override;
  };
}

#endif
