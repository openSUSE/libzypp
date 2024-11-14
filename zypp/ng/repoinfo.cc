/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ng/repoinfo.cc
 *
*/

#include "repoinfo.h"

#include <iostream>
#include <vector>

#include <zypp/ng/ContextBase>

#include <zypp/base/Gettext.h>
#include <zypp/base/LogTools.h>
#include <zypp-core/base/DefaultIntegral>
#include <zypp/parser/xml/XmlEscape.h>

#include <zypp/ManagedFile.h>
#include <zypp/PublicKey.h>
#include <zypp/Glob.h>
#include <zypp/Pathname.h>
#include <zypp/ZConfig.h>
#include <zypp/ExternalProgram.h>

#include <zypp/base/IOStream.h>
#include <zypp-core/base/InputStream>
#include <zypp/parser/xml/Reader.h>

#include <zypp/base/StrMatcher.h>
#include <zypp/KeyRing.h>
#include <zypp/TmpPath.h>
#include <zypp/repo/RepoMirrorList.h>

#include "repoinfoshareddata.h"
#include "zypp/repo/SUSEMediaVerifier.h"

using std::endl;
using zypp::xml::escape;

namespace zyppng {

  namespace
  {
    zypp::repo::RepoType probeCache( const zypp::Pathname & path_r )
    {
      zypp::repo::RepoType ret = zypp::repo::RepoType::NONE;
      if ( zypp::PathInfo(path_r).isDir() )
      {
        if ( zypp::PathInfo(path_r/"/repodata/repomd.xml").isFile() )
        { ret = zypp::repo::RepoType::RPMMD; }
        else if ( zypp::PathInfo(path_r/"/content").isFile() )
        { ret = zypp::repo::RepoType::YAST2; }
        else if ( zypp::PathInfo(path_r/"/cookie").isFile() )
        { ret = zypp::repo::RepoType::RPMPLAINDIR; }
      }
      DBG << "Probed cached type " << ret << " at " << path_r << endl;
      return ret;
    }
  } //

  RepoInfoSharedData::RepoInfoSharedData( zyppng::ContextBaseRef &&context )
    : RepoInfoBaseSharedData( std::move(context) )
    , _rawGpgCheck( zypp::indeterminate )
    , _rawRepoGpgCheck( zypp::indeterminate )
    , _rawPkgGpgCheck( zypp::indeterminate )
    , _validRepoSignature( zypp::indeterminate )
    , _type(zypp::repo::RepoType::NONE_e)
    ,	keeppackages(zypp::indeterminate)
    , _mirrorListForceMetalink(false)
    , emptybaseurls(false)
  {  RepoInfoSharedData::bindVariables(); }

  void RepoInfoSharedData::bindVariables()
  {
    repo::RepoInfoBaseSharedData::bindVariables();
    if ( _ctx ) {
      _mirrorListUrl.setTransformator( repo::RepoVariablesUrlReplacer( zypp::repo::RepoVarRetriever( *_ctx.get() ) )  );
      _baseUrls.setTransformator( repo::RepoVariablesUrlReplacer( zypp::repo::RepoVarRetriever( *_ctx.get() ) )  );
      _gpgKeyUrls.setTransformator( repo::RepoVariablesUrlReplacer( zypp::repo::RepoVarRetriever( *_ctx.get() ) )  );
    } else {
      _mirrorListUrl.setTransformator( repo::RepoVariablesUrlReplacer( nullptr ) );
      _baseUrls.setTransformator( repo::RepoVariablesUrlReplacer( nullptr ) );
      _gpgKeyUrls.setTransformator( repo::RepoVariablesUrlReplacer( nullptr ) );
    }
  }

  void RepoInfoSharedData::setType(const zypp::repo::RepoType &t)
  { _type = t; }

  void RepoInfoSharedData::setProbedType(const zypp::repo::RepoType &t) const
  {
    if (_type == zypp::repo::RepoType::NONE &&
        t != zypp::repo::RepoType::NONE) {
      const_cast<RepoInfoSharedData *>(this)->_type = t;
    }
  }

  zypp::repo::RepoType RepoInfoSharedData::type() const
  {
    if ( _type == zypp::repo::RepoType::NONE && !metadataPath().empty() )
      setProbedType( probeCache( metadataPath() / path ) );
    return _type;
  }

  /** Path to a license tarball in case it exists in the repo. */
  zypp::Pathname RepoInfoSharedData::licenseTgz(const std::string &name_r) const
  {
    zypp::Pathname ret;
    if (!metadataPath().empty()) {
      std::string licenseStem("license");
      if (!name_r.empty()) {
        licenseStem += "-";
        licenseStem += name_r;
      }

      zypp::filesystem::Glob g;
      // TODO: REPOMD: this assumes we know the name of the tarball. In fact
      // we'd need to get the file from repomd.xml (<data
      // type="license[-name_r]">)
      g.add(metadataPath() / path / ("repodata/*" + licenseStem + ".tar.gz"));
      if (g.empty())
        g.add(metadataPath() / path / (licenseStem + ".tar.gz"));

      if (!g.empty())
        ret = *g.begin();
    }
    return ret;
  }

  const RepoVariablesReplacedUrlList & RepoInfoSharedData::baseUrls() const
  {
    const zypp::Url &mlurl(_mirrorListUrl.transformed()); // Variables replaced!
    if (_baseUrls.empty() && !mlurl.asString().empty()) {
      emptybaseurls = true;
      DBG << "MetadataPath: " << metadataPath() << endl;
      zypp::repo::RepoMirrorList rmurls(_ctx, mlurl, metadataPath(),
                                  _mirrorListForceMetalink);
      _baseUrls.raw().insert(_baseUrls.raw().end(), rmurls.getUrls().begin(),
                             rmurls.getUrls().end());
    }
    return _baseUrls;
  }

  RepoVariablesReplacedUrlList &RepoInfoSharedData::baseUrls()
  {
    return _baseUrls;
  }

  bool RepoInfoSharedData::baseurl2dump() const
  {
    return !emptybaseurls && !_baseUrls.empty();
  }

  const RepoVariablesReplacedUrlList &RepoInfoSharedData::gpgKeyUrls() const
  {
    return _gpgKeyUrls;
  }

  RepoVariablesReplacedUrlList &RepoInfoSharedData::gpgKeyUrls()
  {
    return _gpgKeyUrls;
  }

  const std::set<std::string> &RepoInfoSharedData::contentKeywords() const
  {
    hasContent() /*init if not yet done*/;
    return _keywords.second;
  }

  void RepoInfoSharedData::addContent(const std::string &keyword_r)
  {
    _keywords.second.insert(keyword_r);
    if (!hasContent())
      _keywords.first = true;
  }

  bool RepoInfoSharedData::hasContent() const
  {
    if (!_keywords.first && !metadataPath().empty()) {
      // HACK directly check master index file until RepoManager offers
      // some content probing and zypper uses it.
      /////////////////////////////////////////////////////////////////
      MIL << "Empty keywords...." << metadataPath() << endl;
      zypp::Pathname master;
      if (zypp::PathInfo((master = metadataPath() / "/repodata/repomd.xml"))
          .isFile()) {
        // MIL << "GO repomd.." << endl;
        zypp::xml::Reader reader(master);
        while (reader.seekToNode(2, "content")) {
          _keywords.second.insert(reader.nodeText().asString());
          reader.seekToEndNode(2, "content");
        }
        _keywords.first = true; // valid content in _keywords even if empty
      } else if (zypp::PathInfo((master = metadataPath() / "/content")).isFile()) {
        // MIL << "GO content.." << endl;
        zypp::iostr::forEachLine(
              zypp::InputStream(master),
              [this](int num_r, const std::string &line_r) -> bool {
          if (zypp::str::startsWith(line_r, "REPOKEYWORDS")) {
            std::vector<std::string> words;
            if (zypp::str::split(line_r, std::back_inserter(words)) > 1 &&
                words[0].length() == 12 /*"REPOKEYWORDS"*/) {
              this->_keywords.second.insert(++words.begin(), words.end());
            }
            return true; // mult. occurrances are ok.
          }
          return (!zypp::str::startsWith(
                    line_r, "META ")); // no need to parse into META section.
        });
        _keywords.first = true; // valid content in _keywords even if empty
      }
      /////////////////////////////////////////////////////////////////
    }
    return _keywords.first;
  }

  bool RepoInfoSharedData::hasContent(const std::string &keyword_r) const
  {
    return (hasContent() &&
            _keywords.second.find(keyword_r) != _keywords.second.end());
  }

  zypp::TriBool RepoInfoSharedData::internalValidRepoSignature() const
  {
    if (!zypp::indeterminate(_validRepoSignature))
      return _validRepoSignature;
    // check metadata:
    if (!metadataPath().empty()) {
      // A missing ".repo_gpgcheck" might be plaindir(no Downloader) or not yet
      // refreshed signed repo!
      zypp::TriBool linkval = triBoolFromPath(metadataPath() / ".repo_gpgcheck");
      return linkval;
    }
    return zypp::indeterminate;
  }

  void RepoInfoSharedData::internalSetValidRepoSignature( zypp::TriBool value_r)
  {
    if (zypp::PathInfo(metadataPath()).isDir()) {
      zypp::Pathname gpgcheckFile(metadataPath() / ".repo_gpgcheck");
      if (zypp::PathInfo(gpgcheckFile).isExist()) {
        zypp::TriBool linkval(zypp::indeterminate);
        if (triBoolFromPath(gpgcheckFile, linkval) && linkval == value_r)
          return; // existing symlink fits value_r
        else
          zypp::filesystem::unlink(gpgcheckFile); // will write a new one
      }
      zypp::filesystem::symlink(zypp::asString(value_r), gpgcheckFile);
    }
    _validRepoSignature = value_r;
  }

  bool RepoInfoSharedData::internalUnsignedConfirmed() const
  {
    zypp::TriBool linkval(
          true); // want to see it being switched to zypp::indeterminate
    return triBoolFromPath(metadataPath() / ".repo_gpgcheck", linkval) &&
        zypp::indeterminate(linkval);
  }

  bool RepoInfoSharedData::triBoolFromPath(const zypp::Pathname &path_r, zypp::TriBool &ret_r) const
  {
    static const zypp::Pathname truePath("true");
    static const zypp::Pathname falsePath("false");
    static const zypp::Pathname indeterminatePath("zypp::indeterminate");

    // Quiet readlink;
    static const ssize_t bufsiz = 63;
    static char buf[bufsiz + 1];
    ssize_t ret = ::readlink(path_r.c_str(), buf, bufsiz);
    buf[ret == -1 ? 0 : ret] = '\0';

    zypp::Pathname linkval(buf);

    bool known = true;
    if (linkval == truePath)
      ret_r = true;
    else if (linkval == falsePath)
      ret_r = false;
    else if (linkval == indeterminatePath)
      ret_r = zypp::indeterminate;
    else
      known = false;
    return known;
  }

  zypp::TriBool RepoInfoSharedData::triBoolFromPath(const zypp::Pathname &path_r) const
  {
    zypp::TriBool ret(zypp::indeterminate);
    triBoolFromPath(path_r, ret);
    return ret;
  }

  bool RepoInfoSharedData::cfgGpgCheck() const
  {
    const zypp::ZConfig *zConf = nullptr;
    if (!this->_ctx) {
      MIL << "RepoInfo has no context, returning default setting for "
             "cfgRepoGpgCheck!"
          << std::endl;
      zConf = &zypp::ZConfig::defaults();
    } else {
      zConf = &this->_ctx->config();
    }
    return zypp::indeterminate(_rawGpgCheck) ? zConf->gpgCheck()
                                             : (bool)_rawGpgCheck;
  }

  zypp::TriBool RepoInfoSharedData::cfgRepoGpgCheck() const
  {
    const zypp::ZConfig *zConf = nullptr;
    if (!this->_ctx) {
      MIL << "RepoInfo has no context, returning default setting for "
             "cfgRepoGpgCheck!"
          << std::endl;
      zConf = &zypp::ZConfig::defaults();
    } else {
      zConf = &this->_ctx->config();
    }
    return zypp::indeterminate(_rawGpgCheck) &&
        zypp::indeterminate(_rawRepoGpgCheck)
        ? zConf->repoGpgCheck()
        : _rawRepoGpgCheck;
  }

  zypp::TriBool RepoInfoSharedData::cfgPkgGpgCheck() const
  {
    const zypp::ZConfig *zConf = nullptr;
    if (!this->_ctx) {
      MIL << "RepoInfo has no context, returning default setting for "
             "cfgRepoGpgCheck!"
          << std::endl;
      zConf = &zypp::ZConfig::defaults();
    } else {
      zConf = &this->_ctx->config();
    }
    return zypp::indeterminate(_rawGpgCheck) &&
        zypp::indeterminate(_rawPkgGpgCheck)
        ? zConf->pkgGpgCheck()
        : _rawPkgGpgCheck;
  }

  void RepoInfoSharedData::solvPath(zypp::Pathname new_r) {
    _slvPath = std::move(new_r);
  }

  void RepoInfoSharedData::metadataPath(zypp::Pathname new_r) {
    _metadataPath = std::move(new_r);
  }

  void RepoInfoSharedData::packagesPath(zypp::Pathname new_r) {
    _packagesPath = std::move(new_r);
  }

  bool RepoInfoSharedData::usesAutoMetadataPaths() const
  {
    return zypp::str::hasSuffix(_metadataPath.asString(), "/%AUTO%");
  }

  zypp::Pathname RepoInfoSharedData::solvPath() const
  {
    if (usesAutoMetadataPaths())
      return _metadataPath.dirname() / "%SLV%";
    return _slvPath;
  }

  zypp::Pathname RepoInfoSharedData::metadataPath() const
  {
    if (usesAutoMetadataPaths())
      return _metadataPath.dirname() / "%RAW%";
    return _metadataPath;
  }

  zypp::Pathname RepoInfoSharedData::packagesPath() const
  {
    if (_packagesPath.empty() && usesAutoMetadataPaths())
      return _metadataPath.dirname() / "%PKG%";
    return _packagesPath;
  }

  RepoInfoSharedData *RepoInfoSharedData::clone() const
  {
    auto *n = new RepoInfoSharedData(*this);
    n->bindVariables ();
    return n;
  }

  ///////////////////////////////////////////////////////////////////

  /** \relates RepoInfo::Impl Stream output */
  inline std::ostream & operator<<( std::ostream & str, const RepoInfo::Impl & obj )
  {
    return str << "RepoInfo::Impl";
  }

  RepoInfo::RepoInfo( zyppng::ContextBaseRef context )
    : RepoInfoBase( *( new RepoInfoSharedData( std::move(context) ) ) )
  {}

  RepoInfo::~RepoInfo()
  {}

  unsigned RepoInfo::priority() const
  { return pimpl()->priority; }

  const std::optional<RepoInfo> &RepoInfo::nullRepo()
  {
    static std::optional<RepoInfo> _nullRepo;
    return _nullRepo;
  }

  unsigned RepoInfo::defaultPriority()
  { return RepoInfoSharedData::defaultPriority; }

  unsigned RepoInfo::noPriority()
  { return RepoInfoSharedData::noPriority; }

  void RepoInfo::setPriority( unsigned newval_r )
  { pimpl()->priority = newval_r ? newval_r : RepoInfoSharedData::defaultPriority; }

  bool RepoInfo::gpgCheck() const
  { return pimpl()->cfgGpgCheck(); }

  void RepoInfo::setGpgCheck( zypp::TriBool value_r )
  { pimpl()->rawGpgCheck( value_r ); }

  bool RepoInfo::repoGpgCheck() const
  { return gpgCheck() || bool(pimpl()->cfgRepoGpgCheck()); }

  bool RepoInfo::repoGpgCheckIsMandatory() const
  {
    bool ret = ( gpgCheck() && indeterminate(pimpl()->cfgRepoGpgCheck()) ) || bool(pimpl()->cfgRepoGpgCheck());
    if ( ret && pimpl()->internalUnsignedConfirmed() )	// relax if unsigned repo was confirmed in the past
      ret = false;
    return ret;
  }

  void RepoInfo::setRepoGpgCheck( zypp::TriBool value_r )
  { pimpl()->rawRepoGpgCheck( value_r ); }


  bool RepoInfo::pkgGpgCheck() const
  { return bool(pimpl()->cfgPkgGpgCheck()) || ( gpgCheck() && !bool(validRepoSignature())/*enforced*/ ) ; }

  bool RepoInfo::pkgGpgCheckIsMandatory() const
  { return bool(pimpl()->cfgPkgGpgCheck()) || ( gpgCheck() && indeterminate(pimpl()->cfgPkgGpgCheck()) && !bool(validRepoSignature())/*enforced*/ ); }

  void RepoInfo::setPkgGpgCheck( zypp::TriBool value_r )
  { pimpl()->rawPkgGpgCheck( value_r ); }


  void RepoInfo::getRawGpgChecks( zypp::TriBool & g_r, zypp::TriBool & r_r, zypp::TriBool & p_r ) const
  {
    g_r = pimpl()->rawGpgCheck();
    r_r = pimpl()->rawRepoGpgCheck();
    p_r = pimpl()->rawPkgGpgCheck();
  }

  void RepoInfo::setContext( zyppng::ContextBaseRef context )
  {
    pimpl()->switchContext(context);
  }

  RepoInfoSharedData *RepoInfo::pimpl()
  {
    return static_cast<RepoInfoSharedData*>(_pimpl.get());
  }

  const RepoInfoSharedData *RepoInfo::pimpl() const
  {
    return static_cast<const RepoInfoSharedData*>( _pimpl.get() );
  }

  zypp::TriBool RepoInfo::validRepoSignature() const
  {
    zypp::TriBool ret( pimpl()->internalValidRepoSignature() );
    if ( ret && !repoGpgCheck() ) ret = false;	// invalidate any old signature if repoGpgCheck is off
    return ret;
  }

  void RepoInfo::setValidRepoSignature( zypp::TriBool value_r )
  { pimpl()->internalSetValidRepoSignature( value_r ); }

  ///////////////////////////////////////////////////////////////////
  namespace
  {
    inline bool changeGpgCheckTo( zypp::TriBool & lhs, zypp::TriBool rhs )
    { if ( ! sameTriboolState( lhs, rhs ) ) { lhs = rhs; return true; } return false; }

    inline bool changeGpgCheckTo( zypp::TriBool ogpg[3], zypp::TriBool g, zypp::TriBool r, zypp::TriBool p )
    {
      bool changed = false;
      if ( changeGpgCheckTo( ogpg[0], g ) ) changed = true;
      if ( changeGpgCheckTo( ogpg[1], r ) ) changed = true;
      if ( changeGpgCheckTo( ogpg[2], p ) ) changed = true;
      return changed;
    }
  } // namespace
  ///////////////////////////////////////////////////////////////////
  bool RepoInfo::setGpgCheck( GpgCheck mode_r )
  {
    zypp::TriBool ogpg[3];	// Gpg RepoGpg PkgGpg
    getRawGpgChecks( ogpg[0], ogpg[1], ogpg[2] );

    bool changed = false;
    switch ( mode_r )
    {
      case GpgCheck::On:
        changed = changeGpgCheckTo( ogpg, true,               zypp::indeterminate,  zypp::indeterminate );
        break;
      case GpgCheck::Strict:
        changed = changeGpgCheckTo( ogpg, true,               true,                 true          );
        break;
      case GpgCheck::AllowUnsigned:
        changed = changeGpgCheckTo( ogpg, true,               false,                false         );
        break;
      case GpgCheck::AllowUnsignedRepo:
        changed = changeGpgCheckTo( ogpg, true,               false,                zypp::indeterminate );
        break;
      case GpgCheck::AllowUnsignedPackage:
        changed = changeGpgCheckTo( ogpg, true,               zypp::indeterminate,  false         );
        break;
      case GpgCheck::Default:
        changed = changeGpgCheckTo( ogpg, zypp::indeterminate, zypp::indeterminate, zypp::indeterminate );
        break;
      case GpgCheck::Off:
        changed = changeGpgCheckTo( ogpg, false,              zypp::indeterminate,  zypp::indeterminate );
        break;
      case GpgCheck::indeterminate:	// no change
        break;
    }

    if ( changed )
    {
      setGpgCheck    ( ogpg[0] );
      setRepoGpgCheck( ogpg[1] );
      setPkgGpgCheck ( ogpg[2] );
    }
    return changed;
  }

  void RepoInfo::setMirrorListUrl( const zypp::Url & url_r )	// Raw
  { pimpl()->_mirrorListUrl.raw() = url_r; pimpl()->_mirrorListForceMetalink = false; }

  void RepoInfo::setMirrorListUrls( url_set urls )	// Raw
  { setMirrorListUrl( urls.empty() ? zypp::Url() : urls.front() ); }

  void  RepoInfo::setMetalinkUrl( const zypp::Url & url_r )	// Raw
  { pimpl()->_mirrorListUrl.raw() = url_r; pimpl()->_mirrorListForceMetalink = true; }

  void RepoInfo::setMetalinkUrls( url_set urls )	// Raw
  { setMetalinkUrl( urls.empty() ? zypp::Url() : urls.front() ); }

  void RepoInfo::setGpgKeyUrls( url_set urls )
  { pimpl()->gpgKeyUrls().raw().swap( urls ); }

  void RepoInfo::setGpgKeyUrl( const zypp::Url & url_r )
  {
    pimpl()->gpgKeyUrls().raw().clear();
    pimpl()->gpgKeyUrls().raw().push_back( url_r );
  }

  void RepoInfo::addBaseUrl( zypp::Url url_r )
  {
    for ( const auto & url : pimpl()->baseUrls().raw() )	// Raw unique!
      if ( url == url_r )
        return;
    pimpl()->baseUrls().raw().push_back( std::move(url_r) );
  }

  void RepoInfo::setBaseUrl( zypp::Url url_r )
  {
    pimpl()->baseUrls().raw().clear();
    pimpl()->baseUrls().raw().push_back( std::move(url_r) );
  }

  void RepoInfo::setBaseUrls( url_set urls )
  { pimpl()->baseUrls().raw().swap( urls ); }

  void RepoInfo::setPath( const zypp::Pathname &path )
  { pimpl()->path = path; }

  void RepoInfo::setType( const zypp::repo::RepoType &t )
  { pimpl()->setType( t ); }

  void RepoInfo::setProbedType( const zypp::repo::RepoType &t ) const
  { pimpl()->setProbedType( t ); }

  void RepoInfo::setMetadataPath( const zypp::Pathname &path )
  { pimpl()->metadataPath( path ); }

  void RepoInfo::setPackagesPath( const zypp::Pathname &path )
  { pimpl()->packagesPath( path ); }

  void RepoInfo::setSolvCachePath(const zypp::Pathname &path)
  { pimpl()->solvPath (path); }

  void RepoInfo::setKeepPackages( bool keep )
  { pimpl()->keeppackages = keep; }

  void RepoInfo::setService( const std::string& name )
  { pimpl()->service = name; }

  void RepoInfo::setTargetDistribution( const std::string & targetDistribution )
  { pimpl()->targetDistro = targetDistribution; }

  bool RepoInfo::keepPackages() const
  { return indeterminate(pimpl()->keeppackages) ? false : (bool)pimpl()->keeppackages; }

  zypp::Pathname RepoInfo::metadataPath() const
  { return pimpl()->metadataPath(); }

  zypp::Pathname RepoInfo::packagesPath() const
  { return pimpl()->packagesPath(); }

  zypp::Pathname RepoInfo::solvCachePath() const
  { return pimpl()->solvPath(); }

  bool RepoInfo::usesAutoMetadataPaths() const
  { return pimpl()->usesAutoMetadataPaths(); }

  zypp::repo::RepoType RepoInfo::type() const
  { return pimpl()->type(); }

  zypp::Url RepoInfo::mirrorListUrl() const			// Variables replaced!
  { return pimpl()->_mirrorListUrl.transformed(); }

  zypp::Url RepoInfo::rawMirrorListUrl() const		// Raw
  { return pimpl()->_mirrorListUrl.raw(); }

  bool RepoInfo::gpgKeyUrlsEmpty() const
  { return pimpl()->gpgKeyUrls().empty(); }

  RepoInfo::urls_size_type RepoInfo::gpgKeyUrlsSize() const
  { return pimpl()->gpgKeyUrls().size(); }

  RepoInfo::url_set RepoInfo::gpgKeyUrls() const	// Variables replaced!
  { return pimpl()->gpgKeyUrls().transformed(); }

  RepoInfo::url_set RepoInfo::rawGpgKeyUrls() const	// Raw
  { return pimpl()->gpgKeyUrls().raw(); }

  zypp::Url RepoInfo::gpgKeyUrl() const			// Variables replaced!
  { return( pimpl()->gpgKeyUrls().empty() ? zypp::Url() : *pimpl()->gpgKeyUrls().transformedBegin() ); }

  zypp::Url RepoInfo::rawGpgKeyUrl() const			// Raw
  { return( pimpl()->gpgKeyUrls().empty() ? zypp::Url() : *pimpl()->gpgKeyUrls().rawBegin() ) ; }

  RepoInfo::url_set RepoInfo::baseUrls() const		// Variables replaced!
  { return pimpl()->baseUrls().transformed(); }

  RepoInfo::url_set RepoInfo::rawBaseUrls() const	// Raw
  { return pimpl()->baseUrls().raw(); }

  zypp::Pathname RepoInfo::path() const
  { return pimpl()->path; }

  std::string RepoInfo::service() const
  { return pimpl()->service; }

  std::string RepoInfo::targetDistribution() const
  { return pimpl()->targetDistro; }

  zypp::Url RepoInfo::rawUrl() const
  { return( pimpl()->baseUrls().empty() ? zypp::Url() : *pimpl()->baseUrls().rawBegin() ); }

  RepoInfo::urls_const_iterator RepoInfo::baseUrlsBegin() const
  { return pimpl()->baseUrls().transformedBegin(); }

  RepoInfo::urls_const_iterator RepoInfo::baseUrlsEnd() const
  { return pimpl()->baseUrls().transformedEnd(); }

  RepoInfo::urls_size_type RepoInfo::baseUrlsSize() const
  { return pimpl()->baseUrls().size(); }

  bool RepoInfo::baseUrlsEmpty() const
  { return pimpl()->baseUrls().empty(); }

  bool RepoInfo::baseUrlSet() const
  { return pimpl()->baseurl2dump(); }

  const std::set<std::string> & RepoInfo::contentKeywords() const
  { return pimpl()->contentKeywords(); }

  void RepoInfo::addContent( const std::string & keyword_r )
  { pimpl()->addContent( keyword_r ); }

  bool RepoInfo::hasContent() const
  { return pimpl()->hasContent(); }

  bool RepoInfo::hasContent( const std::string & keyword_r ) const
  { return pimpl()->hasContent( keyword_r ); }

  ///////////////////////////////////////////////////////////////////

  bool RepoInfo::hasLicense() const
  { return hasLicense( std::string() ); }

  bool RepoInfo::hasLicense( const std::string & name_r ) const
  { return !pimpl()->licenseTgz( name_r ).empty(); }


  bool RepoInfo::needToAcceptLicense() const
  { return needToAcceptLicense( std::string() ); }

  bool RepoInfo::needToAcceptLicense( const std::string & name_r ) const
  {
    const zypp::Pathname & licenseTgz( pimpl()->licenseTgz( name_r ) );
    if ( licenseTgz.empty() )
      return false;     // no licenses at all

    zypp::ExternalProgram::Arguments cmd;
    cmd.push_back( "tar" );
    cmd.push_back( "-t" );
    cmd.push_back( "-z" );
    cmd.push_back( "-f" );
    cmd.push_back( licenseTgz.asString() );
    zypp::ExternalProgram prog( cmd, zypp::ExternalProgram::Stderr_To_Stdout );

    bool accept = true;
    static const std::string noAcceptanceFile = "no-acceptance-needed\n";
    for ( std::string output( prog.receiveLine() ); output.length(); output = prog.receiveLine() )
    {
      if ( output == noAcceptanceFile )
      {
        accept = false;
      }
    }
    prog.close();
    MIL << "License(" << name_r << ") in " << name() << " has to be accepted: " << (accept?"true":"false" ) << endl;
    return accept;
  }

  std::string RepoInfo::getLicense( const zypp::Locale & lang_r ) const
  { return getLicense( std::string(), lang_r ); }

  std::string RepoInfo::getLicense( const std::string & name_r, const zypp::Locale & lang_r ) const
  {
    zypp::LocaleSet avlocales( getLicenseLocales( name_r ) );
    if ( avlocales.empty() )
      return std::string();

    zypp::Locale getLang( zypp::Locale::bestMatch( avlocales, lang_r ) );
    if ( !getLang && avlocales.find( zypp::Locale::noCode ) == avlocales.end() )
    {
      WAR << "License(" << name_r << ") in " << name() << " contains no fallback text!" << endl;
      // Using the fist locale instead of returning no text at all.
      // So the user might recognize that there is a license, even if they
      // can't read it.
      getLang = *avlocales.begin();
    }

    // now extract the license file.
    static const std::string licenseFileFallback( "license.txt" );
    std::string licenseFile( !getLang ? licenseFileFallback
                                      : zypp::str::form( "license.%s.txt", getLang.c_str() ) );

    zypp::ExternalProgram::Arguments cmd;
    cmd.push_back( "tar" );
    cmd.push_back( "-x" );
    cmd.push_back( "-z" );
    cmd.push_back( "-O" );
    cmd.push_back( "-f" );
    cmd.push_back( pimpl()->licenseTgz( name_r ).asString() ); // if it not exists, avlocales was empty.
    cmd.push_back( licenseFile );

    std::string ret;
    zypp::ExternalProgram prog( cmd, zypp::ExternalProgram::Discard_Stderr );
    for ( std::string output( prog.receiveLine() ); output.length(); output = prog.receiveLine() )
    {
      ret += output;
    }
    prog.close();
    return ret;
  }

  zypp::LocaleSet RepoInfo::getLicenseLocales() const
  { return getLicenseLocales( std::string() ); }

  zypp::LocaleSet RepoInfo::getLicenseLocales( const std::string & name_r ) const
  {
    const zypp::Pathname & licenseTgz( pimpl()->licenseTgz( name_r ) );
    if ( licenseTgz.empty() )
      return zypp::LocaleSet();

    zypp::ExternalProgram::Arguments cmd;
    cmd.push_back( "tar" );
    cmd.push_back( "-t" );
    cmd.push_back( "-z" );
    cmd.push_back( "-f" );
    cmd.push_back( licenseTgz.asString() );

    zypp::LocaleSet ret;
    zypp::ExternalProgram prog( cmd, zypp::ExternalProgram::Stderr_To_Stdout );
    for ( std::string output( prog.receiveLine() ); output.length(); output = prog.receiveLine() )
    {
      static const zypp::C_Str license( "license." );
      static const zypp::C_Str dotTxt( ".txt\n" );
      if ( zypp::str::hasPrefix( output, license ) && zypp::str::hasSuffix( output, dotTxt ) )
      {
        if ( output.size() <= license.size() +  dotTxt.size() ) // license.txt
          ret.insert( zypp::Locale() );
        else
          ret.insert( zypp::Locale( std::string( output.c_str()+license.size(), output.size()- license.size() - dotTxt.size() ) ) );
      }
    }
    prog.close();
    return ret;
  }

  ///////////////////////////////////////////////////////////////////

  std::ostream & RepoInfo::dumpOn( std::ostream & str ) const
  {
    RepoInfoBase::dumpOn(str);
    if ( pimpl()->baseurl2dump() )
    {
      for ( const auto & url : pimpl()->baseUrls().raw() )
      {
        str << "- url         : " << url << std::endl;
      }
    }

    // print if non empty value
    auto strif( [&] ( const std::string & tag_r, const std::string & value_r ) {
      if ( ! value_r.empty() )
        str << tag_r << value_r << std::endl;
    });

    strif( (pimpl()->_mirrorListForceMetalink ? "- metalink    : " : "- mirrorlist  : "), rawMirrorListUrl().asString() );
    strif( "- path        : ", path().asString() );
    str << "- type        : " << type() << std::endl;
    str << "- priority    : " << priority() << std::endl;

    // Yes No Default(Y) Default(N)
#define OUTS(T,B) ( indeterminate(T) ? (std::string("D(")+(B?"Y":"N")+")") : ((bool)T?"Y":"N") )
    str << "- gpgcheck    : " << OUTS(pimpl()->rawGpgCheck(),gpgCheck())
                              << " repo" << OUTS(pimpl()->rawRepoGpgCheck(),repoGpgCheck()) << (repoGpgCheckIsMandatory() ? "* ": " " )
                              << "sig" << zypp::asString( validRepoSignature(), "?", "Y", "N" )
                              << " pkg" << OUTS(pimpl()->rawPkgGpgCheck(),pkgGpgCheck()) << (pkgGpgCheckIsMandatory() ? "* ": " " )
                              << std::endl;
#undef OUTS

    for ( const auto & url : pimpl()->gpgKeyUrls().raw() )
    {
      str << "- gpgkey      : " << url << std::endl;
    }

    if ( ! indeterminate(pimpl()->keeppackages) )
      str << "- keeppackages: " << keepPackages() << std::endl;

    strif( "- service     : ", service() );
    strif( "- targetdistro: ", targetDistribution() );
    strif( "- filePath:     ", filepath().asString() );
    strif( "- metadataPath: ", metadataPath().asString() );
    strif( "- packagesPath: ", packagesPath().asString() );

    return str;
  }

  std::ostream & RepoInfo::dumpAsIniOn( std::ostream & str ) const
  {
    RepoInfoBase::dumpAsIniOn(str);

    if ( pimpl()->baseurl2dump() )
    {
      str << "baseurl=";
      std::string indent;
      for ( const auto & url : pimpl()->baseUrls().raw() )
      {
        str << indent << zypp::hotfix1050625::asString( url ) << endl;
        if ( indent.empty() ) indent = "        ";	// "baseurl="
      }
    }

    if ( ! pimpl()->path.empty() )
      str << "path="<< path() << endl;

    if ( ! (rawMirrorListUrl().asString().empty()) )
      str << (pimpl()->_mirrorListForceMetalink ? "metalink=" : "mirrorlist=") << zypp::hotfix1050625::asString( rawMirrorListUrl() ) << endl;

    if ( type() != zypp::repo::RepoType::NONE )
      str << "type=" << type().asString() << endl;

    if ( priority() != defaultPriority() )
      str << "priority=" << priority() << endl;

    if ( ! indeterminate(pimpl()->rawGpgCheck()) )
      str << "gpgcheck=" << (pimpl()->rawGpgCheck() ? "1" : "0") << endl;

    if ( ! indeterminate(pimpl()->rawRepoGpgCheck()) )
      str << "repo_gpgcheck=" << (pimpl()->rawRepoGpgCheck() ? "1" : "0") << endl;

    if ( ! indeterminate(pimpl()->rawPkgGpgCheck()) )
      str << "pkg_gpgcheck=" << (pimpl()->rawPkgGpgCheck() ? "1" : "0") << endl;

    {
      std::string indent( "gpgkey=");
      for ( const auto & url : pimpl()->gpgKeyUrls().raw() )
      {
        str << indent << url << endl;
        if ( indent[0] != ' ' )
          indent = "       ";
      }
    }

    if (!indeterminate(pimpl()->keeppackages))
      str << "keeppackages=" << keepPackages() << endl;

    if( ! service().empty() )
      str << "service=" << service() << endl;

    return str;
  }

  std::ostream & RepoInfo::dumpAsXmlOn( std::ostream & str, const std::string & content ) const
  {
    std::string tmpstr;
    str
      << "<repo"
      << " alias=\"" << escape(alias()) << "\""
      << " name=\"" << escape(name()) << "\"";
    if (type() != zypp::repo::RepoType::NONE)
      str << " type=\"" << type().asString() << "\"";
    str
      << " priority=\"" << priority() << "\""
      << " enabled=\"" << enabled() << "\""
      << " autorefresh=\"" << autorefresh() << "\""
      << " gpgcheck=\"" << gpgCheck() << "\""
      << " repo_gpgcheck=\"" << repoGpgCheck() << "\""
      << " pkg_gpgcheck=\"" << pkgGpgCheck() << "\"";
    if ( ! indeterminate(pimpl()->rawGpgCheck()) )
      str << " raw_gpgcheck=\"" << (pimpl()->rawGpgCheck() ? "1" : "0") << "\"";
    if ( ! indeterminate(pimpl()->rawRepoGpgCheck()) )
      str << " raw_repo_gpgcheck=\"" << (pimpl()->rawRepoGpgCheck() ? "1" : "0") << "\"";
    if ( ! indeterminate(pimpl()->rawPkgGpgCheck()) )
      str << " raw_pkg_gpgcheck=\"" << (pimpl()->rawPkgGpgCheck() ? "1" : "0") << "\"";
    if (!(tmpstr = gpgKeyUrl().asString()).empty())
    if (!(tmpstr = gpgKeyUrl().asString()).empty())
      str << " gpgkey=\"" << escape(tmpstr) << "\"";
    if (!(tmpstr = mirrorListUrl().asString()).empty())
      str << (pimpl()->_mirrorListForceMetalink ? " metalink=\"" : " mirrorlist=\"") << escape(tmpstr) << "\"";
    str << ">" << endl;

    if ( pimpl()->baseurl2dump() )
    {
      for_( it, baseUrlsBegin(), baseUrlsEnd() )	// !transform iterator replaces variables
        str << "<url>" << escape((*it).asString()) << "</url>" << endl;
    }

    str << "</repo>" << endl;
    return str;
  }


  std::ostream & operator<<( std::ostream & str, const RepoInfo & obj )
  {
    return obj.dumpOn(str);
  }

  bool RepoInfo::requireStatusWithMediaFile () const
  {
    // We skip the check for downloading media unless a local copy of the
    // media file exists and states that there is more than one medium.
    bool canSkipMediaCheck = std::all_of( baseUrlsBegin(), baseUrlsEnd(), []( const zypp::Url &url ) { return url.schemeIsDownloading(); });
    if ( canSkipMediaCheck ) {
      const auto &mDataPath = metadataPath();
      if ( not mDataPath.empty() ) {
        zypp::PathInfo mediafile { mDataPath/"media.1/media" };
        if ( mediafile.isExist() ) {
          zypp::repo::SUSEMediaVerifier lverifier { mediafile.path() };
          if ( lverifier && lverifier.totalMedia() > 1 ) {
            canSkipMediaCheck = false;
          }
        }
      }
    }
    if ( canSkipMediaCheck )
      DBG << "Can SKIP media.1/media check for status calc of repo " << alias() << endl;
    return not canSkipMediaCheck;
  }
} // namespace zyppng
