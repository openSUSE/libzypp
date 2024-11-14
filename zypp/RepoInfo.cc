/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/RepoInfo.cc
 *
*/

#include "RepoInfo.h"

#include <zypp/zypp_detail/ZYppImpl.h>
#include <zypp/ng/repoinfoshareddata.h>
#include <zypp/ng/repoinfo.h>

namespace zypp {

  ZYPP_BEGIN_LEGACY_API

  const RepoInfo RepoInfo::noRepo( zyppng::RepoInfo(nullptr) );

  RepoInfo::RepoInfo( )
    : _pimpl( std::make_unique<zyppng::RepoInfo>( zypp_detail::GlobalStateHelper::context() ) )
  {}

  RepoInfo::RepoInfo(zyppng::RepoInfo *pimpl)
    : _pimpl(pimpl)
    , _ownsImpl(false)
  {}

  RepoInfo::~RepoInfo()
  {
    if ( !_ownsImpl )
      _pimpl.release();
  }

  RepoInfo::RepoInfo( const zyppng::RepoInfo &pimpl )
    : _pimpl( std::make_unique<zyppng::RepoInfo>(pimpl) )
    , _ownsImpl( true )
  { }

  RepoInfo::RepoInfo(const RepoInfo &other)
    : _pimpl( std::make_unique<zyppng::RepoInfo>(*other._pimpl) )
    , _ownsImpl( true )
  {}

  RepoInfo::RepoInfo(RepoInfo &&other)
    : _pimpl( std::move(other._pimpl) )
    , _ownsImpl( other._ownsImpl )
  {}

  zyppng::repo::RepoInfoBase &RepoInfo::pimpl()
  {
    return *_pimpl;
  }

  const zyppng::repo::RepoInfoBase &RepoInfo::pimpl() const
  {
    return *_pimpl;
  }

  RepoInfo &RepoInfo::operator=(const RepoInfo &other)
  {
    _pimpl = std::make_unique<zyppng::RepoInfo>(*other._pimpl);
    _ownsImpl = true;
    return *this;
  }

  RepoInfo &RepoInfo::operator=( RepoInfo &&other )
  {
    _pimpl = std::move(other._pimpl);
    _ownsImpl = other._ownsImpl;
    return *this;
  }

  unsigned RepoInfo::priority() const
  { return _pimpl->priority(); }

  unsigned RepoInfo::defaultPriority()
  { return zyppng::RepoInfoSharedData::defaultPriority; }

  unsigned RepoInfo::noPriority()
  { return zyppng::RepoInfoSharedData::noPriority; }

  void RepoInfo::setPriority( unsigned newval_r )
  { _pimpl->setPriority(newval_r); }

  bool RepoInfo::gpgCheck() const
  { return _pimpl->gpgCheck(); }

  void RepoInfo::setGpgCheck( zypp::TriBool value_r )
  { _pimpl->setGpgCheck(value_r); }

  void RepoInfo::setGpgCheck(bool value_r)
  { _pimpl->setGpgCheck(zypp::TriBool(value_r)); }

  bool RepoInfo::repoGpgCheck() const
  { return _pimpl->repoGpgCheck(); }

  bool RepoInfo::repoGpgCheckIsMandatory() const
  { return _pimpl->repoGpgCheckIsMandatory (); }

  void RepoInfo::setRepoGpgCheck( zypp::TriBool value_r )
  { _pimpl->setRepoGpgCheck(value_r); }

  bool RepoInfo::pkgGpgCheck() const
  { return _pimpl->pkgGpgCheck(); }

  bool RepoInfo::pkgGpgCheckIsMandatory() const
  { return _pimpl->pkgGpgCheckIsMandatory(); }

  void RepoInfo::setPkgGpgCheck( zypp::TriBool value_r )
  { _pimpl->setPkgGpgCheck(value_r); }

  void RepoInfo::getRawGpgChecks( zypp::TriBool & g_r, zypp::TriBool & r_r, zypp::TriBool & p_r ) const
  { _pimpl->getRawGpgChecks( g_r, r_r, p_r ); }

  zypp::TriBool RepoInfo::validRepoSignature() const
  { return _pimpl->validRepoSignature(); }

  void RepoInfo::setValidRepoSignature( zypp::TriBool value_r )
  { _pimpl->setValidRepoSignature (value_r ); }

  bool RepoInfo::setGpgCheck( GpgCheck mode_r )
  { return _pimpl->setGpgCheck(mode_r); }

  void RepoInfo::setMirrorListUrl( const zypp::Url & url_r )	// Raw
  { _pimpl->setMirrorListUrl(url_r); }

  void RepoInfo::setMirrorListUrls( url_set urls )	// Raw
  { _pimpl->setMirrorListUrls(std::move(urls)); }

  void  RepoInfo::setMetalinkUrl( const zypp::Url & url_r )	// Raw
  { _pimpl->setMetalinkUrl(url_r); }

  void RepoInfo::setMetalinkUrls( url_set urls )	// Raw
  { _pimpl->setMetalinkUrls(std::move(urls)); }

  void RepoInfo::setGpgKeyUrls( url_set urls )
  { _pimpl->setGpgKeyUrls(std::move(urls)); }

  void RepoInfo::setGpgKeyUrl( const zypp::Url & url_r )
  { _pimpl->setGpgKeyUrl(url_r); }

  void RepoInfo::addBaseUrl( zypp::Url url_r )
  { _pimpl->addBaseUrl( std::move(url_r) ); }

  void RepoInfo::setBaseUrl( zypp::Url url_r )
  { _pimpl->setBaseUrl( std::move(url_r) ); }

  void RepoInfo::setBaseUrls( url_set urls )
  { _pimpl->setBaseUrls(std::move(urls)); }

  void RepoInfo::setPath( const zypp::Pathname &path )
  { _pimpl->setPath(path); }

  void RepoInfo::setType( const zypp::repo::RepoType &t )
  { _pimpl->setType(t); }

  void RepoInfo::setProbedType( const zypp::repo::RepoType &t ) const
  { _pimpl->setProbedType(t); }

  void RepoInfo::setMetadataPath( const zypp::Pathname &path )
  { _pimpl->setMetadataPath(path); }

  void RepoInfo::setPackagesPath( const zypp::Pathname &path )
  { _pimpl->setPackagesPath(path); }

  void RepoInfo::setSolvCachePath(const zypp::Pathname &path)
  { _pimpl->setSolvCachePath(path); }

  void RepoInfo::setKeepPackages( bool keep )
  { _pimpl->setKeepPackages( keep ); }

  void RepoInfo::setService( const std::string& name )
  { _pimpl->setService(name); }

  void RepoInfo::setTargetDistribution( const std::string & targetDistribution )
  { _pimpl->setTargetDistribution( targetDistribution ); }

  bool RepoInfo::keepPackages() const
  { return _pimpl->keepPackages(); }

  zypp::Pathname RepoInfo::metadataPath() const
  { return _pimpl->metadataPath(); }

  zypp::Pathname RepoInfo::packagesPath() const
  { return _pimpl->packagesPath(); }

  zypp::Pathname RepoInfo::solvCachePath() const
  { return _pimpl->solvCachePath(); }

  bool RepoInfo::usesAutoMetadataPaths() const
  { return _pimpl->usesAutoMetadataPaths(); }

  zypp::repo::RepoType RepoInfo::type() const
  { return _pimpl->type(); }

  zypp::Url RepoInfo::mirrorListUrl() const
  { return _pimpl->mirrorListUrl(); }

  zypp::Url RepoInfo::rawMirrorListUrl() const		// Raw
  { return _pimpl->rawMirrorListUrl(); }

  bool RepoInfo::gpgKeyUrlsEmpty() const
  { return _pimpl->gpgKeyUrlsEmpty(); }

  RepoInfo::urls_size_type RepoInfo::gpgKeyUrlsSize() const
  { return _pimpl->gpgKeyUrlsSize(); }

  RepoInfo::url_set RepoInfo::gpgKeyUrls() const	// Variables replaced!
  { return _pimpl->gpgKeyUrls(); }

  RepoInfo::url_set RepoInfo::rawGpgKeyUrls() const	// Raw
  { return _pimpl->rawGpgKeyUrls(); }

  zypp::Url RepoInfo::gpgKeyUrl() const			// Variables replaced!
  { return _pimpl->gpgKeyUrl(); }

  zypp::Url RepoInfo::rawGpgKeyUrl() const			// Raw
  { return _pimpl->rawGpgKeyUrl(); }

  RepoInfo::url_set RepoInfo::baseUrls() const		// Variables replaced!
  { return _pimpl->baseUrls(); }

  RepoInfo::url_set RepoInfo::rawBaseUrls() const	// Raw
  { return _pimpl->rawBaseUrls(); }

  zypp::Pathname RepoInfo::path() const
  { return _pimpl->path(); }

  std::string RepoInfo::service() const
  { return _pimpl->service(); }

  std::string RepoInfo::targetDistribution() const
  { return _pimpl->targetDistribution(); }

  zypp::Url RepoInfo::rawUrl() const
  { return _pimpl->rawUrl(); }

  RepoInfo::urls_const_iterator RepoInfo::baseUrlsBegin() const
  { return _pimpl->baseUrlsBegin(); }

  RepoInfo::urls_const_iterator RepoInfo::baseUrlsEnd() const
  { return _pimpl->baseUrlsEnd(); }

  RepoInfo::urls_size_type RepoInfo::baseUrlsSize() const
  { return _pimpl->baseUrlsSize(); }

  bool RepoInfo::baseUrlsEmpty() const
  { return _pimpl->baseUrlsEmpty(); }

  bool RepoInfo::baseUrlSet() const
  { return _pimpl->baseUrlSet(); }

  const std::set<std::string> & RepoInfo::contentKeywords() const
  { return _pimpl->contentKeywords(); }

  void RepoInfo::addContent( const std::string & keyword_r )
  { _pimpl->addContent( keyword_r ); }

  bool RepoInfo::hasContent() const
  { return _pimpl->hasContent(); }

  bool RepoInfo::hasContent( const std::string & keyword_r ) const
  { return _pimpl->hasContent( keyword_r ); }

  bool RepoInfo::hasLicense() const
  { return _pimpl->hasLicense( ); }

  bool RepoInfo::hasLicense( const std::string & name_r ) const
  { return _pimpl->hasLicense(name_r); }

  bool RepoInfo::needToAcceptLicense() const
  { return _pimpl->needToAcceptLicense(); }

  bool RepoInfo::needToAcceptLicense( const std::string & name_r ) const
  { return _pimpl->needToAcceptLicense( name_r ); }

  std::string RepoInfo::getLicense( const zypp::Locale & lang_r )
  { return _pimpl->getLicense(lang_r); }

  std::string RepoInfo::getLicense( const zypp::Locale & lang_r ) const
  { return _pimpl->getLicense (lang_r); }

  std::string RepoInfo::getLicense( const std::string & name_r, const zypp::Locale & lang_r ) const
  { return _pimpl->getLicense ( name_r, lang_r ); }

  zypp::LocaleSet RepoInfo::getLicenseLocales() const
  { return _pimpl->getLicenseLocales(); }

  zypp::LocaleSet RepoInfo::getLicenseLocales( const std::string & name_r ) const
  { return _pimpl->getLicenseLocales( name_r ); }

  std::ostream & RepoInfo::dumpOn( std::ostream & str ) const
  { return _pimpl->dumpOn( str ); }

  std::ostream & RepoInfo::dumpAsIniOn( std::ostream & str ) const
  { return _pimpl->dumpAsIniOn( str ); }

  std::ostream & RepoInfo::dumpAsXmlOn( std::ostream & str, const std::string & content ) const
  { return _pimpl->dumpAsXmlOn ( str, content );}

  std::ostream & operator<<( std::ostream & str, const RepoInfo & obj )
  { return obj.dumpOn(str); }

  bool RepoInfo::requireStatusWithMediaFile () const
  { return _pimpl->requireStatusWithMediaFile(); }

  zyppng::RepoInfo &RepoInfo::ngRepoInfo()
  { return *_pimpl; }

  const zyppng::RepoInfo &RepoInfo::ngRepoInfo() const
  { return *_pimpl; }

  ZYPP_END_LEGACY_API
}
