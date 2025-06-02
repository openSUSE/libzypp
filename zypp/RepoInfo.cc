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
#include <zypp/ng/workflows/contextfacade.h>
#include <iostream>
#include <vector>

#include <zypp/base/Gettext.h>
#include <zypp/base/LogTools.h>
#include <zypp-core/base/DefaultIntegral>
#include <zypp/parser/xml/XmlEscape.h>

#include <zypp/ManagedFile.h>
#include <zypp-common/PublicKey.h>
#include <zypp/MediaSetAccess.h>
#include <zypp/RepoInfo.h>
#include <zypp/Glob.h>
#include <zypp/TriBool.h>
#include <zypp/Pathname.h>
#include <zypp/ZConfig.h>
#include <zypp/repo/RepoMirrorList.h>
#include <zypp/repo/SUSEMediaVerifier.h>
#include <zypp/ExternalProgram.h>

#include <zypp/base/IOStream.h>
#include <zypp-core/base/InputStream>
#include <zypp/parser/xml/Reader.h>


#include <zypp/base/StrMatcher.h>
#include <zypp/KeyRing.h>
#include <zypp/TmpPath.h>
#include <zypp/ZYppFactory.h>
#include <zypp/ZYppCallbacks.h>

#include <zypp/ng/workflows/repoinfowf.h>
#include <zypp-curl/private/curlhelper_p.h>

using std::endl;
using zypp::xml::escape;

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  namespace
  {
    repo::RepoType probeCache( const Pathname & path_r )
    {
      repo::RepoType ret = repo::RepoType::NONE;
      if ( PathInfo(path_r).isDir() )
      {
        if ( PathInfo(path_r/"/repodata/repomd.xml").isFile() )
        { ret = repo::RepoType::RPMMD; }
        else if ( PathInfo(path_r/"/content").isFile() )
        { ret = repo::RepoType::YAST2; }
        else if ( PathInfo(path_r/"/cookie").isFile() )
        { ret = repo::RepoType::RPMPLAINDIR; }
      }
      DBG << "Probed cached type " << ret << " at " << path_r << endl;
      return ret;
    }
  } // namespace

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : RepoInfo::Impl
  //
  /** RepoInfo implementation. */
  struct RepoInfo::Impl
  {
    Impl()
      : _rawGpgCheck( indeterminate )
      , _rawRepoGpgCheck( indeterminate )
      , _rawPkgGpgCheck( indeterminate )
      , _validRepoSignature( indeterminate )
      , _type(repo::RepoType::NONE_e)
      ,	keeppackages(indeterminate)
    {}

    Impl(const Impl &) = default;
    Impl(Impl &&) = delete;
    Impl &operator=(const Impl &) = delete;
    Impl &operator=(Impl &&) = delete;

    ~Impl() {}

  public:
    static const unsigned defaultPriority = 99;
    static const unsigned noPriority = unsigned(-1);

    void setType( const repo::RepoType & t )
    { _type = t; }

    void setProbedType( const repo::RepoType & t ) const
    {
      if ( _type == repo::RepoType::NONE && t != repo::RepoType::NONE )
      { const_cast<Impl*>(this)->_type = t; }
    }

    repo::RepoType type() const
    {
      if ( _type == repo::RepoType::NONE && not metadataPath().empty() )
        setProbedType( probeCache( metadataPath() / path ) );
      return _type;
    }

  public:
    /** Path to a license tarball in case it exists in the repo. */
    Pathname licenseTgz( const std::string & name_r ) const
    {
      Pathname ret;
      if ( !metadataPath().empty() )
      {
        std::string licenseStem( "license" );
        if ( !name_r.empty() )
        {
          licenseStem += "-";
          licenseStem += name_r;
        }

        filesystem::Glob g;
        // TODO: REPOMD: this assumes we know the name of the tarball. In fact
        // we'd need to get the file from repomd.xml (<data type="license[-name_r]">)
        g.add( metadataPath() / path / ("repodata/*"+licenseStem+".tar.gz") );
        if ( g.empty() )
          g.add( metadataPath() / path / (licenseStem+".tar.gz") );

        if ( !g.empty() )
          ret = *g.begin();
      }
      return ret;
    }

    RepoVariablesReplacedUrl baseUrl() const {
      if ( !_baseUrls.empty() ) {
        return RepoVariablesReplacedUrl( _baseUrls.raw ().front(), _baseUrls.transformator() );
      }
      if ( !effectiveBaseUrls().empty() ) {
        return RepoVariablesReplacedUrl( effectiveBaseUrls ().front (), _baseUrls.transformator() );
      }
      return RepoVariablesReplacedUrl();
    }

    const RepoVariablesReplacedUrlList & baseUrls() const
    {
      return _baseUrls;
    }

    Url location() const {
      if ( !_baseUrls.empty() )
        return *_baseUrls.transformedBegin ();
      return mirrorListUrl().transformed();
    }

    void resetEffectiveUrls() const {
      _effectiveBaseUrls.clear ();
      _lastEffectiveUrlsUpdate = std::chrono::steady_clock::time_point::min();
    }

    url_set &effectiveBaseUrls() const
    {
      if ( !_effectiveBaseUrls.empty()
           && ( std::chrono::steady_clock::now() - _lastEffectiveUrlsUpdate < std::chrono::hours(1) ) )
        return _effectiveBaseUrls;

      _effectiveBaseUrls.clear();
      _lastEffectiveUrlsUpdate = std::chrono::steady_clock::now();

      bool isAutoMirrorList = false; // bsc#1243901 Allows mirrorlist parsing to fail if automatically switched on

      Url mlurl( mirrorListUrl().transformed() );	// Variables replaced!
      if ( mlurl.asString().empty()
           && _baseUrls.raw().size() == 1
           && repo::RepoMirrorList::urlSupportsMirrorLink( *_baseUrls.transformedBegin() ) ) {

        mlurl = *_baseUrls.transformedBegin ();
        mlurl.appendPathName("/");
        mlurl.setQueryParam("mirrorlist", std::string() );

        MIL << "Detected opensuse.org baseUrl with no mirrors, requesting them from : " << mlurl.asString() << std::endl;
        isAutoMirrorList = true;
      }

      if ( !mlurl.asString().empty() )
      {
        try {
          DBG << "MetadataPath: " << metadataPath() << endl;
          repo::RepoMirrorList rmurls( mlurl, metadataPath() );

          // propagate internally used URL params like 'proxy' to the mirrors
          const auto &tf = [urlTemplate =mirrorListUrl().transformed()]( const zypp::Url &in ){
            return internal::propagateQueryParams ( in , urlTemplate );
          };
          _effectiveBaseUrls.insert( _effectiveBaseUrls.end(), make_transform_iterator( rmurls.getUrls().begin(), tf ), make_transform_iterator( rmurls.getUrls().end(), tf ) );
        } catch ( const zypp::Exception & e ) {
          // failed to fetch the mirrorlist/metalink, if we still have a baseUrl we can go on, otherwise this is a error
          if ( _baseUrls.empty () )
            throw;
          else {
            MIL << "Mirrorlist failed, repo either returns invalid data or has no mirrors at all, falling back to only baseUrl!" << std::endl;
            if ( !isAutoMirrorList ) {
              callback::UserData data( JobReport::repoRefreshMirrorlist );
              data.set("error", e );
              JobReport::warning( _("Failed to fetch mirrorlist/metalink data."), data );
            }
          }
        }
      }

      _effectiveBaseUrls.insert( _effectiveBaseUrls.end(), _baseUrls.transformedBegin (), _baseUrls.transformedEnd () );
      return _effectiveBaseUrls;
    }

    std::vector<std::vector<Url>> groupedBaseUrls() const
    {
      // here we group the URLs to figure out mirrors
      std::vector<std::vector<Url>> urlGroups;
      int dlUrlIndex = -1; //we remember the index of the first downloading URL

      const auto &baseUrls = effectiveBaseUrls();
      std::for_each ( baseUrls.begin(), baseUrls.end(), [&]( const zypp::Url &url ){
        if ( !url.schemeIsDownloading () ) {
          urlGroups.push_back ( { url } );
          return;
        }

        if ( dlUrlIndex >= 0) {
          urlGroups[dlUrlIndex].push_back ( url );
          return;
        }

        // start a new group
        urlGroups.push_back ( {url} );
        dlUrlIndex = urlGroups.size() - 1;
      });

      return urlGroups;
    }

    RepoVariablesReplacedUrlList & baseUrls()
    { return _baseUrls; }

    bool baseurl2dump() const
    { return !_baseUrls.empty(); }


    const RepoVariablesReplacedUrlList & gpgKeyUrls() const
    { return _gpgKeyUrls; }

    RepoVariablesReplacedUrlList & gpgKeyUrls()
    { return _gpgKeyUrls; }

    std::string repoStatusString() const
    {
      if ( mirrorListUrl().transformed().isValid() )
        return mirrorListUrl().transformed().asString();
      if ( !baseUrls().empty() )
        return (*baseUrls().transformedBegin()).asString();
      return std::string();
    }

    const std::set<std::string> & contentKeywords() const
    { hasContent()/*init if not yet done*/; return _keywords.second; }

    void addContent( const std::string & keyword_r )
    { _keywords.second.insert( keyword_r ); if ( ! hasContent() ) _keywords.first = true; }

    bool hasContent() const
    {
      if ( !_keywords.first && ! metadataPath().empty() )
      {
        // HACK directly check master index file until RepoManager offers
        // some content probing and zypper uses it.
        /////////////////////////////////////////////////////////////////
        MIL << "Empty keywords...." << metadataPath() << endl;
        Pathname master;
        if ( PathInfo( (master=metadataPath()/"/repodata/repomd.xml") ).isFile() )
        {
          //MIL << "GO repomd.." << endl;
          xml::Reader reader( master );
          while ( reader.seekToNode( 2, "content" ) )
          {
            _keywords.second.insert( reader.nodeText().asString() );
            reader.seekToEndNode( 2, "content" );
          }
          _keywords.first = true;	// valid content in _keywords even if empty
        }
        else if ( PathInfo( (master=metadataPath()/"/content") ).isFile() )
        {
          //MIL << "GO content.." << endl;
          iostr::forEachLine( InputStream( master ),
                            [this]( int num_r, const std::string& line_r )->bool
                            {
                              if ( str::startsWith( line_r, "REPOKEYWORDS" ) )
                              {
                                std::vector<std::string> words;
                                if ( str::split( line_r, std::back_inserter(words) ) > 1
                                  && words[0].length() == 12 /*"REPOKEYWORDS"*/ )
                                {
                                  this->_keywords.second.insert( ++words.begin(), words.end() );
                                }
                                return true; // mult. occurrances are ok.
                              }
                              return( ! str::startsWith( line_r, "META " ) );	// no need to parse into META section.
                            } );
          _keywords.first = true;	// valid content in _keywords even if empty
        }
        /////////////////////////////////////////////////////////////////
      }
      return _keywords.first;
    }

    bool hasContent( const std::string & keyword_r ) const
    { return( hasContent() && _keywords.second.find( keyword_r ) != _keywords.second.end() ); }

    /** Signature check result needs to be stored/retrieved from _metadataPath.
     * Don't call them from outside validRepoSignature/setValidRepoSignature
     */
    //@{
    TriBool internalValidRepoSignature() const
    {
      if ( ! indeterminate(_validRepoSignature) )
        return _validRepoSignature;
      // check metadata:
      if ( ! metadataPath().empty() )
      {
        // A missing ".repo_gpgcheck" might be plaindir(no Downloader) or not yet refreshed signed repo!
        TriBool linkval = triBoolFromPath( metadataPath() / ".repo_gpgcheck" );
        return linkval;
      }
      return indeterminate;
    }

    void internalSetValidRepoSignature( TriBool value_r )
    {
      if ( PathInfo(metadataPath()).isDir() )
      {
        Pathname gpgcheckFile( metadataPath() / ".repo_gpgcheck" );
        if ( PathInfo(gpgcheckFile).isExist() )
        {
          TriBool linkval( indeterminate );
          if ( triBoolFromPath( gpgcheckFile, linkval ) && linkval == value_r )
            return;	// existing symlink fits value_r
          else
            filesystem::unlink( gpgcheckFile );	// will write a new one
        }
        filesystem::symlink( asString(value_r), gpgcheckFile );
      }
      _validRepoSignature = value_r;
    }

    /** We definitely have a symlink pointing to "indeterminate" (for repoGpgCheckIsMandatory)?
     * I.e. user accepted the unsigned repo in Downloader. A test whether `internalValidRepoSignature`
     * is indeterminate would include not yet checked repos, which is unwanted here.
     */
    bool internalUnsignedConfirmed() const
    {
      TriBool linkval( true );	// want to see it being switched to indeterminate
      return triBoolFromPath( metadataPath() / ".repo_gpgcheck", linkval ) && indeterminate(linkval);
    }

    bool triBoolFromPath( const Pathname & path_r, TriBool & ret_r ) const
    {
      static const Pathname truePath( "true" );
      static const Pathname falsePath( "false" );
      static const Pathname indeterminatePath( "indeterminate" );

      // Quiet readlink;
      static const ssize_t bufsiz = 63;
      static char buf[bufsiz+1];
      ssize_t ret = ::readlink( path_r.c_str(), buf, bufsiz );
      buf[ret == -1 ? 0 : ret] = '\0';

      Pathname linkval( buf );

      bool known = true;
      if ( linkval == truePath )
        ret_r = true;
      else if ( linkval == falsePath )
        ret_r = false;
      else if ( linkval == indeterminatePath )
        ret_r = indeterminate;
      else
        known = false;
      return known;
    }

    TriBool triBoolFromPath( const Pathname & path_r ) const
    { TriBool ret(indeterminate); triBoolFromPath( path_r, ret ); return ret; }

    //@}

  private:
    TriBool _rawGpgCheck;	///< default gpgcheck behavior: Y/N/ZConf
    TriBool _rawRepoGpgCheck;	///< need to check repo sign.: Y/N/(ZConf(Y/N/gpgCheck))
    TriBool _rawPkgGpgCheck;	///< need to check pkg sign.: Y/N/(ZConf(Y/N/gpgCheck))

  public:
    TriBool rawGpgCheck() const			{ return _rawGpgCheck; }
    TriBool rawRepoGpgCheck() const		{ return _rawRepoGpgCheck; }
    TriBool rawPkgGpgCheck() const		{ return _rawPkgGpgCheck; }

    void rawGpgCheck( TriBool val_r )		{ _rawGpgCheck = val_r; }
    void rawRepoGpgCheck( TriBool val_r )	{ _rawRepoGpgCheck = val_r; }
    void rawPkgGpgCheck( TriBool val_r )	{ _rawPkgGpgCheck = val_r; }

    bool cfgGpgCheck() const
    { return indeterminate(_rawGpgCheck) ? ZConfig::instance().gpgCheck() : (bool)_rawGpgCheck; }
    TriBool cfgRepoGpgCheck() const
    { return indeterminate(_rawGpgCheck) && indeterminate(_rawRepoGpgCheck) ? ZConfig::instance().repoGpgCheck() : _rawRepoGpgCheck; }
    TriBool cfgPkgGpgCheck() const
    { return indeterminate(_rawGpgCheck) && indeterminate(_rawPkgGpgCheck) ? ZConfig::instance().pkgGpgCheck() : _rawPkgGpgCheck; }

  private:
    TriBool _validRepoSignature; ///< have  signed and valid repo metadata
    repo::RepoType _type;

  private:
    RepoVariablesReplacedUrl _cfgMirrorlistUrl;
    RepoVariablesReplacedUrl _cfgMetalinkUrl;
  public:
    /** THE mirrorListUrl to work with (either_cfgMirrorlistUrl or _cfgMetalinkUrl) */
    const RepoVariablesReplacedUrl & mirrorListUrl() const
    { return _cfgMirrorlistUrl.transformed().isValid() ? _cfgMirrorlistUrl : _cfgMetalinkUrl; }

    void setMirrorlistUrl( const Url & url_r )	// Raw
    { _cfgMirrorlistUrl.raw() = url_r; }

    void setMetalinkUrl( const Url & url_r )	// Raw
    { _cfgMetalinkUrl.raw() = url_r; }

    /** Config file writing needs to tell them appart. */
    const RepoVariablesReplacedUrl & cfgMirrorlistUrl() const
    { return _cfgMirrorlistUrl; }
    /** Config file writing needs to tell them appart. */
    const RepoVariablesReplacedUrl & cfgMetalinkUrl() const
    { return _cfgMetalinkUrl; }

  public:
    TriBool keeppackages;
    Pathname path;
    std::string service;
    std::string targetDistro;

    void metadataPath( Pathname new_r )
    { _metadataPath = std::move( new_r ); }

    void packagesPath( Pathname new_r )
    { _packagesPath = std::move( new_r ); }

    bool usesAutoMetadataPaths() const
    { return str::hasSuffix( _metadataPath.asString(), "/%AUTO%" ); }

    Pathname metadataPath() const
    {
      if ( usesAutoMetadataPaths() )
        return _metadataPath.dirname() / "%RAW%";
      return _metadataPath;
    }

    Pathname packagesPath() const
    {
      if ( _packagesPath.empty() && usesAutoMetadataPaths() )
        return _metadataPath.dirname() / "%PKG%";
      return _packagesPath;
    }

    Pathname predownloadPath() const
    {
      return packagesPath() / ".preload";
    }

    DefaultIntegral<unsigned,defaultPriority> priority;

  private:
    Pathname _metadataPath;
    Pathname _packagesPath;

    mutable RepoVariablesReplacedUrlList _baseUrls;
    mutable url_set _effectiveBaseUrls;
    mutable std::chrono::steady_clock::time_point _lastEffectiveUrlsUpdate = std::chrono::steady_clock::time_point::min();
    mutable std::pair<FalseBool, std::set<std::string> > _keywords;

    RepoVariablesReplacedUrlList _gpgKeyUrls;

    friend Impl * rwcowClone<Impl>( const Impl * rhs );
    /** clone for RWCOW_pointer */
    Impl * clone() const
    { return new Impl( *this ); }
  };
  ///////////////////////////////////////////////////////////////////

  /** \relates RepoInfo::Impl Stream output */
  inline std::ostream & operator<<( std::ostream & str, const RepoInfo::Impl & obj )
  {
    return str << "RepoInfo::Impl";
  }

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : RepoInfo
  //
  ///////////////////////////////////////////////////////////////////

  const RepoInfo RepoInfo::noRepo;

  RepoInfo::RepoInfo()
  : _pimpl( new Impl() )
  {}

  RepoInfo::~RepoInfo()
  {}

  unsigned RepoInfo::priority() const
  { return _pimpl->priority; }

  unsigned RepoInfo::defaultPriority()
  { return Impl::defaultPriority; }

  unsigned RepoInfo::noPriority()
  { return Impl::noPriority; }

  void RepoInfo::setPriority( unsigned newval_r )
  { _pimpl->priority = newval_r ? newval_r : Impl::defaultPriority; }


  bool RepoInfo::gpgCheck() const
  { return _pimpl->cfgGpgCheck(); }

  void RepoInfo::setGpgCheck( TriBool value_r )
  { _pimpl->rawGpgCheck( value_r ); }

  void RepoInfo::setGpgCheck( bool value_r ) // deprecated legacy and for squid
  { setGpgCheck( TriBool(value_r) ); }


  bool RepoInfo::repoGpgCheck() const
  { return gpgCheck() || bool(_pimpl->cfgRepoGpgCheck()); }

  bool RepoInfo::repoGpgCheckIsMandatory() const
  {
    bool ret = ( gpgCheck() && indeterminate(_pimpl->cfgRepoGpgCheck()) ) || bool(_pimpl->cfgRepoGpgCheck());
    if ( ret && _pimpl->internalUnsignedConfirmed() )	// relax if unsigned repo was confirmed in the past
      ret = false;
    return ret;
  }

  void RepoInfo::setRepoGpgCheck( TriBool value_r )
  { _pimpl->rawRepoGpgCheck( value_r ); }


  bool RepoInfo::pkgGpgCheck() const
  { return bool(_pimpl->cfgPkgGpgCheck()) || ( gpgCheck() && !bool(validRepoSignature())/*enforced*/ ) ; }

  bool RepoInfo::pkgGpgCheckIsMandatory() const
  { return bool(_pimpl->cfgPkgGpgCheck()) || ( gpgCheck() && indeterminate(_pimpl->cfgPkgGpgCheck()) && !bool(validRepoSignature())/*enforced*/ ); }

  void RepoInfo::setPkgGpgCheck( TriBool value_r )
  { _pimpl->rawPkgGpgCheck( value_r ); }


  void RepoInfo::getRawGpgChecks( TriBool & g_r, TriBool & r_r, TriBool & p_r ) const
  {
    g_r = _pimpl->rawGpgCheck();
    r_r = _pimpl->rawRepoGpgCheck();
    p_r = _pimpl->rawPkgGpgCheck();
  }


  TriBool RepoInfo::validRepoSignature() const
  {
    TriBool ret( _pimpl->internalValidRepoSignature() );
    if ( ret && !repoGpgCheck() ) ret = false;	// invalidate any old signature if repoGpgCheck is off
    return ret;
  }

  void RepoInfo::setValidRepoSignature( TriBool value_r )
  { _pimpl->internalSetValidRepoSignature( value_r ); }

  ///////////////////////////////////////////////////////////////////
  namespace
  {
    inline bool changeGpgCheckTo( TriBool & lhs, TriBool rhs )
    { if ( ! sameTriboolState( lhs, rhs ) ) { lhs = rhs; return true; } return false; }

    inline bool changeGpgCheckTo( TriBool ogpg[3], TriBool g, TriBool r, TriBool p )
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
    TriBool ogpg[3];	// Gpg RepoGpg PkgGpg
    getRawGpgChecks( ogpg[0], ogpg[1], ogpg[2] );

    bool changed = false;
    switch ( mode_r )
    {
      case GpgCheck::On:
        changed = changeGpgCheckTo( ogpg, true,          indeterminate, indeterminate );
        break;
      case GpgCheck::Strict:
        changed = changeGpgCheckTo( ogpg, true,          true,          true          );
        break;
      case GpgCheck::AllowUnsigned:
        changed = changeGpgCheckTo( ogpg, true,          false,         false         );
        break;
      case GpgCheck::AllowUnsignedRepo:
        changed = changeGpgCheckTo( ogpg, true,          false,         indeterminate );
        break;
      case GpgCheck::AllowUnsignedPackage:
        changed = changeGpgCheckTo( ogpg, true,          indeterminate, false         );
        break;
      case GpgCheck::Default:
        changed = changeGpgCheckTo( ogpg, indeterminate, indeterminate, indeterminate );
        break;
      case GpgCheck::Off:
        changed = changeGpgCheckTo( ogpg, false,         indeterminate, indeterminate );
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

  void RepoInfo::setMirrorlistUrl( const Url & url_r )	// Raw
  { _pimpl->setMirrorlistUrl( url_r ); }

  void  RepoInfo::setMetalinkUrl( const Url & url_r )	// Raw
  { _pimpl->setMetalinkUrl( url_r ); }

  Url RepoInfo::rawCfgMirrorlistUrl() const
  { return _pimpl->cfgMirrorlistUrl().raw(); }

  Url RepoInfo::rawCfgMetalinkUrl() const
  { return _pimpl->cfgMetalinkUrl().raw(); }

#if LEGACY(1735)
  void RepoInfo::setMirrorListUrl( const Url & url_r )	// Raw
  { setMirrorlistUrl( url_r ); }
  void RepoInfo::setMirrorListUrls( url_set urls )	// Raw
  { _pimpl->setMirrorlistUrl( urls.empty() ? Url() : urls.front() ); }
  void RepoInfo::setMetalinkUrls( url_set urls )	// Raw
  { _pimpl->setMetalinkUrl( urls.empty() ? Url() : urls.front() ); }
#endif

  void RepoInfo::setGpgKeyUrls( url_set urls )
  { _pimpl->gpgKeyUrls().raw().swap( urls ); }

  void RepoInfo::setGpgKeyUrl( const Url & url_r )
  {
    _pimpl->gpgKeyUrls().raw().clear();
    _pimpl->gpgKeyUrls().raw().push_back( url_r );
  }

  std::string RepoInfo::repoStatusString() const
  { return _pimpl->repoStatusString(); }

  void RepoInfo::addBaseUrl( Url url_r )
  {
    for ( const auto & url : _pimpl->baseUrls().raw() )	// Raw unique!
      if ( url == url_r )
        return;

    _pimpl->baseUrls().raw().push_back( url_r );
    _pimpl->resetEffectiveUrls ();
  }

  void RepoInfo::setBaseUrl( Url url_r )
  {
    _pimpl->baseUrls().raw().clear();
    _pimpl->resetEffectiveUrls ();
    _pimpl->baseUrls().raw().push_back( std::move(url_r) );
  }

  void RepoInfo::setBaseUrls( url_set urls )
  {
    _pimpl->resetEffectiveUrls ();
    _pimpl->baseUrls().raw().swap( urls );
  }

  bool RepoInfo::effectiveBaseUrlsEmpty() const
  {
    return _pimpl->effectiveBaseUrls().empty();
  }

  RepoInfo::url_set RepoInfo::effectiveBaseUrls() const
  {
    return _pimpl->effectiveBaseUrls();
  }

  void RepoInfo::setPath( const Pathname &path )
  { _pimpl->path = path; }

  void RepoInfo::setType( const repo::RepoType &t )
  { _pimpl->setType( t ); }

  void RepoInfo::setProbedType( const repo::RepoType &t ) const
  { _pimpl->setProbedType( t ); }


  void RepoInfo::setMetadataPath( const Pathname &path )
  { _pimpl->metadataPath( path ); }

  void RepoInfo::setPackagesPath( const Pathname &path )
  { _pimpl->packagesPath( path ); }

  Pathname RepoInfo::predownloadPath() const
  { return _pimpl->predownloadPath(); }

  void RepoInfo::setKeepPackages( bool keep )
  { _pimpl->keeppackages = keep; }

  void RepoInfo::setService( const std::string& name )
  { _pimpl->service = name; }

  void RepoInfo::setTargetDistribution( const std::string & targetDistribution )
  { _pimpl->targetDistro = targetDistribution; }

  bool RepoInfo::keepPackages() const
  { return indeterminate(_pimpl->keeppackages) ? false : (bool)_pimpl->keeppackages; }

  bool RepoInfo::effectiveKeepPackages() const
  { return keepPackages() || PathInfo(packagesPath().dirname()/".keep_packages").isExist(); }

  Pathname RepoInfo::metadataPath() const
  { return _pimpl->metadataPath(); }

  Pathname RepoInfo::packagesPath() const
  { return _pimpl->packagesPath(); }

  bool RepoInfo::usesAutoMetadataPaths() const
  { return _pimpl->usesAutoMetadataPaths(); }

  repo::RepoType RepoInfo::type() const
  { return _pimpl->type(); }

  Url RepoInfo::mirrorListUrl() const			// Variables replaced!
  { return _pimpl->mirrorListUrl().transformed(); }

  Url RepoInfo::rawMirrorListUrl() const		// Raw
  { return _pimpl->mirrorListUrl().raw(); }

  bool RepoInfo::gpgKeyUrlsEmpty() const
  { return _pimpl->gpgKeyUrls().empty(); }

  RepoInfo::urls_size_type RepoInfo::gpgKeyUrlsSize() const
  { return _pimpl->gpgKeyUrls().size(); }

  RepoInfo::url_set RepoInfo::gpgKeyUrls() const	// Variables replaced!
  { return _pimpl->gpgKeyUrls().transformed(); }

  RepoInfo::url_set RepoInfo::rawGpgKeyUrls() const	// Raw
  { return _pimpl->gpgKeyUrls().raw(); }

  Url RepoInfo::gpgKeyUrl() const			// Variables replaced!
  { return( _pimpl->gpgKeyUrls().empty() ? Url() : *_pimpl->gpgKeyUrls().transformedBegin() ); }

  Url RepoInfo::rawGpgKeyUrl() const			// Raw
  { return( _pimpl->gpgKeyUrls().empty() ? Url() : *_pimpl->gpgKeyUrls().rawBegin() ) ; }

  RepoInfo::url_set RepoInfo::baseUrls() const		// Variables replaced!
  { return _pimpl->baseUrls().transformed(); }

  RepoInfo::url_set RepoInfo::rawBaseUrls() const	// Raw
  { return _pimpl->baseUrls().raw(); }

  Pathname RepoInfo::path() const
  { return _pimpl->path; }

  std::string RepoInfo::service() const
  { return _pimpl->service; }

  std::string RepoInfo::targetDistribution() const
  { return _pimpl->targetDistro; }

  Url RepoInfo::rawUrl() const
  { return _pimpl->baseUrl().raw(); }

  Url RepoInfo::location() const
  { return _pimpl->location (); }

  std::vector<std::vector<Url>> RepoInfo::groupedBaseUrls() const
  { return _pimpl->groupedBaseUrls(); }

  RepoInfo::urls_const_iterator RepoInfo::baseUrlsBegin() const
  { return _pimpl->baseUrls().transformedBegin(); }

  RepoInfo::urls_const_iterator RepoInfo::baseUrlsEnd() const
  { return _pimpl->baseUrls().transformedEnd(); }

  RepoInfo::urls_size_type RepoInfo::baseUrlsSize() const
  { return _pimpl->baseUrls().size(); }

  bool RepoInfo::baseUrlsEmpty() const
  { return _pimpl->baseUrls().empty(); }

  bool RepoInfo::baseUrlSet() const
  { return _pimpl->baseurl2dump(); }

  Url RepoInfo::url() const
  {
    return _pimpl->baseUrl().transformed();
  }

  const std::set<std::string> & RepoInfo::contentKeywords() const
  { return _pimpl->contentKeywords(); }

  void RepoInfo::addContent( const std::string & keyword_r )
  { _pimpl->addContent( keyword_r ); }

  bool RepoInfo::hasContent() const
  { return _pimpl->hasContent(); }

  bool RepoInfo::hasContent( const std::string & keyword_r ) const
  { return _pimpl->hasContent( keyword_r ); }

  ///////////////////////////////////////////////////////////////////

  bool RepoInfo::hasLicense() const
  { return hasLicense( std::string() ); }

  bool RepoInfo::hasLicense( const std::string & name_r ) const
  { return !_pimpl->licenseTgz( name_r ).empty(); }


  bool RepoInfo::needToAcceptLicense() const
  { return needToAcceptLicense( std::string() ); }

  bool RepoInfo::needToAcceptLicense( const std::string & name_r ) const
  {
    const Pathname & licenseTgz( _pimpl->licenseTgz( name_r ) );
    if ( licenseTgz.empty() )
      return false;     // no licenses at all

    ExternalProgram::Arguments cmd;
    cmd.push_back( "tar" );
    cmd.push_back( "-t" );
    cmd.push_back( "-z" );
    cmd.push_back( "-f" );
    cmd.push_back( licenseTgz.asString() );
    ExternalProgram prog( cmd, ExternalProgram::Stderr_To_Stdout );

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


  std::string RepoInfo::getLicense( const Locale & lang_r )
  { return const_cast<const RepoInfo *>(this)->getLicense( std::string(), lang_r ); }

  std::string RepoInfo::getLicense( const Locale & lang_r ) const
  { return getLicense( std::string(), lang_r ); }

  std::string RepoInfo::getLicense( const std::string & name_r, const Locale & lang_r ) const
  {
    LocaleSet avlocales( getLicenseLocales( name_r ) );
    if ( avlocales.empty() )
      return std::string();

    Locale getLang( Locale::bestMatch( avlocales, lang_r ) );
    if ( !getLang && avlocales.find( Locale::noCode ) == avlocales.end() )
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
                                      : str::form( "license.%s.txt", getLang.c_str() ) );

    ExternalProgram::Arguments cmd;
    cmd.push_back( "tar" );
    cmd.push_back( "-x" );
    cmd.push_back( "-z" );
    cmd.push_back( "-O" );
    cmd.push_back( "-f" );
    cmd.push_back( _pimpl->licenseTgz( name_r ).asString() ); // if it not exists, avlocales was empty.
    cmd.push_back( licenseFile );

    std::string ret;
    ExternalProgram prog( cmd, ExternalProgram::Discard_Stderr );
    for ( std::string output( prog.receiveLine() ); output.length(); output = prog.receiveLine() )
    {
      ret += output;
    }
    prog.close();
    return ret;
  }


  LocaleSet RepoInfo::getLicenseLocales() const
  { return getLicenseLocales( std::string() ); }

  LocaleSet RepoInfo::getLicenseLocales( const std::string & name_r ) const
  {
    const Pathname & licenseTgz( _pimpl->licenseTgz( name_r ) );
    if ( licenseTgz.empty() )
      return LocaleSet();

    ExternalProgram::Arguments cmd;
    cmd.push_back( "tar" );
    cmd.push_back( "-t" );
    cmd.push_back( "-z" );
    cmd.push_back( "-f" );
    cmd.push_back( licenseTgz.asString() );

    LocaleSet ret;
    ExternalProgram prog( cmd, ExternalProgram::Stderr_To_Stdout );
    for ( std::string output( prog.receiveLine() ); output.length(); output = prog.receiveLine() )
    {
      static const C_Str license( "license." );
      static const C_Str dotTxt( ".txt\n" );
      if ( str::hasPrefix( output, license ) && str::hasSuffix( output, dotTxt ) )
      {
        if ( output.size() <= license.size() +  dotTxt.size() ) // license.txt
          ret.insert( Locale() );
        else
          ret.insert( Locale( std::string( output.c_str()+license.size(), output.size()- license.size() - dotTxt.size() ) ) );
      }
    }
    prog.close();
    return ret;
  }

  ///////////////////////////////////////////////////////////////////

  std::ostream & RepoInfo::dumpOn( std::ostream & str ) const
  {
    RepoInfoBase::dumpOn(str);
    if ( _pimpl->baseurl2dump() )
    {
      for ( const auto & url : _pimpl->baseUrls().raw() )
      {
        str << "- url         : " << url << std::endl;
      }
    }

    // print if non empty value
    auto strif( [&] ( const std::string & tag_r, const std::string & value_r ) {
      if ( ! value_r.empty() )
        str << tag_r << value_r << std::endl;
    });

    strif( "- mirrorlist  : ", _pimpl->cfgMirrorlistUrl().raw().asString() );
    strif( "- metalink    : ", _pimpl->cfgMetalinkUrl().raw().asString() );
    strif( "- path        : ", path().asString() );
    str << "- type        : " << type() << std::endl;
    str << "- priority    : " << priority() << std::endl;

    // Yes No Default(Y) Default(N)
#define OUTS(T,B) ( indeterminate(T) ? (std::string("D(")+(B?"Y":"N")+")") : ((bool)T?"Y":"N") )
    str << "- gpgcheck    : " << OUTS(_pimpl->rawGpgCheck(),gpgCheck())
                              << " repo" << OUTS(_pimpl->rawRepoGpgCheck(),repoGpgCheck()) << (repoGpgCheckIsMandatory() ? "* ": " " )
                              << "sig" << asString( validRepoSignature(), "?", "Y", "N" )
                              << " pkg" << OUTS(_pimpl->rawPkgGpgCheck(),pkgGpgCheck()) << (pkgGpgCheckIsMandatory() ? "* ": " " )
                              << std::endl;
#undef OUTS

    for ( const auto & url : _pimpl->gpgKeyUrls().raw() )
    {
      str << "- gpgkey      : " << url << std::endl;
    }

    if ( ! indeterminate(_pimpl->keeppackages) )
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
    // libzypp/#638: Add a note to service maintained repo entries
    if( ! service().empty() ) {
      str << "# Repository '"<<alias()<<"' is maintained by the '"<<service()<<"' service." << endl;
      str << "# Manual changes may be overwritten by a service refresh." << endl;
      str << "# See also 'man zypper', section 'Services'." << endl;
    }
    RepoInfoBase::dumpAsIniOn(str);

    if ( _pimpl->baseurl2dump() )
    {
      str << "baseurl=";
      std::string indent;
      for ( const auto & url : _pimpl->baseUrls().raw() )
      {
        str << indent << hotfix1050625::asString( url ) << endl;
        if ( indent.empty() ) indent = "        ";	// "baseurl="
      }
    }

    if ( ! _pimpl->path.empty() )
      str << "path="<< path() << endl;

    if ( ! _pimpl->cfgMirrorlistUrl().raw().asString().empty() )
      str << "mirrorlist=" << hotfix1050625::asString( _pimpl->cfgMirrorlistUrl().raw() ) << endl;

    if ( ! _pimpl->cfgMetalinkUrl().raw().asString().empty() )
      str << "metalink=" << hotfix1050625::asString( _pimpl->cfgMetalinkUrl().raw() ) << endl;

    if ( type() != repo::RepoType::NONE )
      str << "type=" << type().asString() << endl;

    if ( priority() != defaultPriority() )
      str << "priority=" << priority() << endl;

    if ( ! indeterminate(_pimpl->rawGpgCheck()) )
      str << "gpgcheck=" << (_pimpl->rawGpgCheck() ? "1" : "0") << endl;

    if ( ! indeterminate(_pimpl->rawRepoGpgCheck()) )
      str << "repo_gpgcheck=" << (_pimpl->rawRepoGpgCheck() ? "1" : "0") << endl;

    if ( ! indeterminate(_pimpl->rawPkgGpgCheck()) )
      str << "pkg_gpgcheck=" << (_pimpl->rawPkgGpgCheck() ? "1" : "0") << endl;

    {
      std::string indent( "gpgkey=");
      for ( const auto & url : _pimpl->gpgKeyUrls().raw() )
      {
        str << indent << url << endl;
        if ( indent[0] != ' ' )
          indent = "       ";
      }
    }

    if (!indeterminate(_pimpl->keeppackages))
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
    if (type() != repo::RepoType::NONE)
      str << " type=\"" << type().asString() << "\"";
    str
      << " priority=\"" << priority() << "\""
      << " enabled=\"" << enabled() << "\""
      << " autorefresh=\"" << autorefresh() << "\""
      << " gpgcheck=\"" << gpgCheck() << "\""
      << " repo_gpgcheck=\"" << repoGpgCheck() << "\""
      << " pkg_gpgcheck=\"" << pkgGpgCheck() << "\"";
    if ( ! indeterminate(_pimpl->rawGpgCheck()) )
      str << " raw_gpgcheck=\"" << (_pimpl->rawGpgCheck() ? "1" : "0") << "\"";
    if ( ! indeterminate(_pimpl->rawRepoGpgCheck()) )
      str << " raw_repo_gpgcheck=\"" << (_pimpl->rawRepoGpgCheck() ? "1" : "0") << "\"";
    if ( ! indeterminate(_pimpl->rawPkgGpgCheck()) )
      str << " raw_pkg_gpgcheck=\"" << (_pimpl->rawPkgGpgCheck() ? "1" : "0") << "\"";
    if (!(tmpstr = gpgKeyUrl().asString()).empty())
      str << " gpgkey=\"" << escape(tmpstr) << "\"";
    if ( ! (tmpstr = _pimpl->cfgMirrorlistUrl().transformed().asString()).empty() )
      str << " mirrorlist=\"" << escape(tmpstr) << "\"";
    if ( ! (tmpstr = _pimpl->cfgMetalinkUrl().transformed().asString()).empty() )
      str << " metalink=\"" << escape(tmpstr) << "\"";
    str << ">" << endl;

    if ( _pimpl->baseurl2dump() )
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

  std::ostream & operator<<( std::ostream & str, const RepoInfo::GpgCheck & obj )
  {
    switch ( obj )
    {
#define OUTS( V ) case RepoInfo::V: return str << #V; break
      OUTS( GpgCheck::On );
      OUTS( GpgCheck::Strict );
      OUTS( GpgCheck::AllowUnsigned );
      OUTS( GpgCheck::AllowUnsignedRepo );
      OUTS( GpgCheck::AllowUnsignedPackage );
      OUTS( GpgCheck::Default );
      OUTS( GpgCheck::Off );
      OUTS( GpgCheck::indeterminate );
#undef OUTS
    }
    return str << "GpgCheck::UNKNOWN";
  }

  bool RepoInfo::requireStatusWithMediaFile () const
  {
    // We skip the check for downloading media unless a local copy of the
    // media file exists and states that there is more than one medium.
    const auto &effUrls = _pimpl->effectiveBaseUrls ();
    bool canSkipMediaCheck = std::all_of( effUrls.begin(), effUrls.end(), []( const zypp::Url &url ) { return url.schemeIsDownloading(); });
    if ( canSkipMediaCheck ) {
      const auto &mDataPath = metadataPath();
      if ( not mDataPath.empty() ) {
        PathInfo mediafile { mDataPath/"media.1/media" };
        if ( mediafile.isExist() ) {
          repo::SUSEMediaVerifier lverifier { mediafile.path() };
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

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
