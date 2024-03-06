/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/zypp_detail/repomanagerbase_p.h
 *
*/
#ifndef ZYPP_ZYPP_DETAIL_REPOMANAGERIMPL_H
#define ZYPP_ZYPP_DETAIL_REPOMANAGERIMPL_H

#include <zypp/RepoManagerOptions.h>
#include <zypp/RepoStatus.h>

#include <zypp/repo/RepoException.h>

#include <zypp-core/base/Gettext.h>
#include <zypp-core/base/DefaultIntegral>
#include <zypp-core/base/NonCopyable.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/ui/progressdata.h>

namespace zypp {

  #define OPT_PROGRESS const ProgressData::ReceiverFnc & = ProgressData::ReceiverFnc()

  /** Whether repo is not under RM control and provides its own methadata paths. */
  inline bool isTmpRepo( const RepoInfo & info_r )
  { return( info_r.filepath().empty() && info_r.usesAutoMetadataPaths() ); }

  void assert_alias( const RepoInfo & info );

  inline void assert_alias( const ServiceInfo & info )
  {
    if ( info.alias().empty() )
      ZYPP_THROW( repo::ServiceNoAliasException( info ) );
    // bnc #473834. Maybe we can match the alias against a regex to define
    // and check for valid aliases
    if ( info.alias()[0] == '.')
      ZYPP_THROW(repo::ServiceInvalidAliasException(
        info, _("Service alias cannot start with dot.")));
  }

  /** Check if alias_r is present in repo/service container. */
  template <class Iterator>
  inline bool foundAliasIn( const std::string & alias_r, Iterator begin_r, Iterator end_r )
  {
    for_( it, begin_r, end_r )
      if ( it->alias() == alias_r )
      return true;
    return false;
  }
  /** \overload */
  template <class Container>
  inline bool foundAliasIn( const std::string & alias_r, const Container & cont_r )
  { return foundAliasIn( alias_r, cont_r.begin(), cont_r.end() ); }

  /** Find alias_r in repo/service container. */
  template <class Iterator>
  inline Iterator findAlias( const std::string & alias_r, Iterator begin_r, Iterator end_r )
  {
    for_( it, begin_r, end_r )
      if ( it->alias() == alias_r )
      return it;
    return end_r;
  }
  /** \overload */
  template <class Container>
  inline typename Container::iterator findAlias( const std::string & alias_r, Container & cont_r )
  { return findAlias( alias_r, cont_r.begin(), cont_r.end() ); }
  /** \overload */
  template <class Container>
  inline typename Container::const_iterator findAlias( const std::string & alias_r, const Container & cont_r )
  { return findAlias( alias_r, cont_r.begin(), cont_r.end() ); }


  /** \short Generate a related filename from a repo/service infos alias */
  std::string filenameFromAlias( const std::string & alias_r, const std::string & stem_r );

  /**
     * \short Simple callback to collect the results
     *
     * Classes like RepoFileReader call the callback
     * once per each repo in a file.
     *
     * Passing this functor as callback, you can collect
     * all results at the end, without dealing with async
     * code.
     *
     * If targetDistro is set, all repos with non-empty RepoInfo::targetDistribution()
     * will be skipped.
     *
     * \todo do this through a separate filter
     */
  struct RepoCollector : private base::NonCopyable
  {
    RepoCollector()
    {}

    RepoCollector(const std::string & targetDistro_)
      : targetDistro(targetDistro_)
    {}

    bool collect( const RepoInfo &repo );

    RepoInfoList repos;
    std::string targetDistro;
  };
  ////////////////////////////////////////////////////////////////////////////

  /**
     * Reads RepoInfo's from a repo file.
     *
     * \param file pathname of the file to read.
     */
  std::list<RepoInfo> repositories_in_file( const Pathname & file );

  ////////////////////////////////////////////////////////////////////////////

  /**
     * \short List of RepoInfo's from a directory
     *
     * Goes trough every file ending with ".repo" in a directory and adds all
     * RepoInfo's contained in that file.
     *
     * \param dir pathname of the directory to read.
     */
  std::list<RepoInfo> repositories_in_dir( const Pathname &dir );

  void assert_urls( const RepoInfo & info );

  inline void assert_url( const ServiceInfo & info )
  {
    if ( ! info.url().isValid() )
      ZYPP_THROW( repo::ServiceNoUrlException( info ) );
  }

  /**
     * \short Calculates the raw cache path for a repository, this is usually
     * /var/cache/zypp/alias
     */
  inline Pathname rawcache_path_for_repoinfo( const RepoManagerOptions &opt, const RepoInfo &info )
  {
    assert_alias(info);
    return isTmpRepo( info ) ? info.metadataPath() : opt.repoRawCachePath / info.escaped_alias();
  }

  /**
     * \short Calculates the raw product metadata path for a repository, this is
     * inside the raw cache dir, plus an optional path where the metadata is.
     *
     * It should be different only for repositories that are not in the root of
     * the media.
     * for example /var/cache/zypp/alias/addondir
     */
  inline Pathname rawproductdata_path_for_repoinfo( const RepoManagerOptions &opt, const RepoInfo &info )
  { return rawcache_path_for_repoinfo( opt, info ) / info.path(); }

  /**
     * \short Calculates the packages cache path for a repository
     */
  inline Pathname packagescache_path_for_repoinfo( const RepoManagerOptions &opt, const RepoInfo &info )
  {
    assert_alias(info);
    return isTmpRepo( info ) ? info.packagesPath() : opt.repoPackagesCachePath / info.escaped_alias();
  }

  /**
     * \short Calculates the solv cache path for a repository
     */
  inline Pathname solv_path_for_repoinfo( const RepoManagerOptions &opt, const RepoInfo &info )
  {
    assert_alias(info);
    return isTmpRepo( info ) ? info.metadataPath().dirname() / "%SLV%" : opt.repoSolvCachePath / info.escaped_alias();
  }

  ////////////////////////////////////////////////////////////////////////////

  /** Functor collecting ServiceInfos into a ServiceSet. */
  class ServiceCollector
  {
  public:
    typedef std::set<ServiceInfo> ServiceSet;

    ServiceCollector( ServiceSet & services_r )
      : _services( services_r )
    {}

    bool operator()( const ServiceInfo & service_r ) const
    {
      _services.insert( service_r );
      return true;
    }

  private:
    ServiceSet & _services;
  };
  ////////////////////////////////////////////////////////////////////////////

  /** bsc#1204956: Tweak to prevent auto pruning package caches. */
  bool autoPruneInDir( const Pathname & path_r );


  ///////////////////////////////////////////////////////////////////
  /// \class RepoManager::Impl
  /// \brief RepoManager implementation.
  ///
  ///////////////////////////////////////////////////////////////////
  struct ZYPP_LOCAL RepoManagerBaseImpl
  {
  public:

    /**
     * Functor thats filter RepoInfo by service which it belongs to.
     */
    struct MatchServiceAlias
    {
    public:
      MatchServiceAlias( const std::string & alias_ ) : alias(alias_) {}
      bool operator()( const RepoInfo & info ) const
      { return info.service() == alias; }
    private:
      std::string alias;
    };

    /** ServiceInfo typedefs */
    using ServiceSet = std::set<ServiceInfo>;
    using ServiceConstIterator = ServiceSet::const_iterator;
    using ServiceSizeType = ServiceSet::size_type;

    /** RepoInfo typedefs */
    using RepoSet = std::set<RepoInfo>;
    using RepoConstIterator = RepoSet::const_iterator;
    using RepoSizeType = RepoSet::size_type;


    RepoManagerBaseImpl( const RepoManagerOptions &opt );
    virtual ~RepoManagerBaseImpl();

  public:

    bool repoEmpty() const		{ return repos().empty(); }
    RepoSizeType repoSize() const	{ return repos().size(); }
    RepoConstIterator repoBegin() const	{ return repos().begin(); }
    RepoConstIterator repoEnd() const	{ return repos().end(); }

    bool hasRepo( const std::string & alias ) const
    { return foundAliasIn( alias, repos() ); }

    RepoInfo getRepo( const std::string & alias ) const
    {
      RepoConstIterator it( findAlias( alias, repos() ) );
      return it == repos().end() ? RepoInfo::noRepo : *it;
    }

  public:
    Pathname metadataPath( const RepoInfo & info ) const
    { return rawcache_path_for_repoinfo( _options, info ); }

    Pathname packagesPath( const RepoInfo & info ) const
    { return packagescache_path_for_repoinfo( _options, info ); }

    static RepoStatus metadataStatus( const RepoInfo & info, const RepoManagerOptions &options );
    RepoStatus metadataStatus( const RepoInfo & info ) const;

    void cleanMetadata( const RepoInfo & info, OPT_PROGRESS );

    void cleanPackages(const RepoInfo & info, OPT_PROGRESS , bool isAutoClean = false);

    static repo::RepoType probeCache( const Pathname & path_r );

    void cleanCacheDirGarbage( OPT_PROGRESS );

    void cleanCache( const RepoInfo & info, OPT_PROGRESS );

    bool isCached( const RepoInfo & info ) const
    { return PathInfo(solv_path_for_repoinfo( _options, info ) / "solv").isExist(); }

    RepoStatus cacheStatus( const RepoInfo & info ) const
    { return cacheStatus( info, _options ); }

    static RepoStatus cacheStatus( const RepoInfo & info, const RepoManagerOptions &options )
    { return RepoStatus::fromCookieFile(solv_path_for_repoinfo(options, info) / "cookie"); }

    void loadFromCache( const RepoInfo & info, OPT_PROGRESS );

    void addProbedRepository( const RepoInfo & info, repo::RepoType probedType );

    virtual void removeRepository ( const RepoInfo & info, OPT_PROGRESS ) = 0;

    void modifyRepository( const std::string & alias, const RepoInfo & newinfo_r, OPT_PROGRESS );

    RepoInfo getRepositoryInfo( const std::string & alias );
    RepoInfo getRepositoryInfo( const Url & url, const url::ViewOption & urlview );

  public:
    bool serviceEmpty() const			{ return _services.empty(); }
    ServiceSizeType serviceSize() const		{ return _services.size(); }
    ServiceConstIterator serviceBegin() const	{ return _services.begin(); }
    ServiceConstIterator serviceEnd() const	{ return _services.end(); }

    bool hasService( const std::string & alias ) const
    { return foundAliasIn( alias, _services ); }

    ServiceInfo getService( const std::string & alias ) const
    {
      ServiceConstIterator it( findAlias( alias, _services ) );
      return it == _services.end() ? ServiceInfo::noService : *it;
    }

  public:
    void addService( const ServiceInfo & service );
    void addService( const std::string & alias, const Url & url )
    { addService( ServiceInfo( alias, url ) ); }

    void removeService( const std::string & alias );
    void removeService( const ServiceInfo & service )
    { removeService( service.alias() ); }

    void modifyService( const std::string & oldAlias, const ServiceInfo & newService );

    static void touchIndexFile( const RepoInfo & info, const RepoManagerOptions &options );

  protected:
    void removeRepositoryImpl( const RepoInfo & info, OPT_PROGRESS );
    void saveService( ServiceInfo & service ) const;

    Pathname generateNonExistingName( const Pathname & dir, const std::string & basefilename ) const;

    std::string generateFilename( const RepoInfo & info ) const
    { return filenameFromAlias( info.alias(), "repo" ); }

    std::string generateFilename( const ServiceInfo & info ) const
    { return filenameFromAlias( info.alias(), "service" ); }

    void setCacheStatus( const RepoInfo & info, const RepoStatus & status )
    {
      Pathname base = solv_path_for_repoinfo( _options, info );
      filesystem::assert_dir(base);
      status.saveToCookieFile( base / "cookie" );
    }

    void touchIndexFile( const RepoInfo & info );

    template<typename OutputIterator>
    void getRepositoriesInService( const std::string & alias, OutputIterator out ) const
    {
      MatchServiceAlias filter( alias );
      std::copy( boost::make_filter_iterator( filter, repos().begin(), repos().end() ),
        boost::make_filter_iterator( filter, repos().end(), repos().end() ),
        out);
    }

  protected:
    void init_knownServices();
    void init_knownRepositories();

    const RepoSet & repos() const { return _reposX; }
    RepoSet & reposManip()        { if ( ! _reposDirty ) _reposDirty = true; return _reposX; }

  protected:
    RepoManagerOptions	_options;
    RepoSet 		_reposX;
    ServiceSet		_services;

    DefaultIntegral<bool,false> _reposDirty;
  };

}

#endif
