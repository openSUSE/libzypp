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
#ifndef ZYPP_NG_REPOMANAGER_INCLUDED
#define ZYPP_NG_REPOMANAGER_INCLUDED

#include <utility>

#include <zypp/RepoManagerFlags.h>
#include <zypp/RepoManagerOptions.h>
#include <zypp/RepoStatus.h>

#include <zypp/repo/RepoException.h>
#include <zypp/repo/PluginRepoverification.h>
#include <zypp/ng/workflows/logichelpers.h>

#include <zypp-core/base/Gettext.h>
#include <zypp-core/base/DefaultIntegral>
#include <zypp-core/base/NonCopyable.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/ui/progressdata.h>
#include <zypp-core/zyppng/base/Base>
#include <zypp-core/zyppng/base/zyppglobal.h>
#include <zypp-core/zyppng/pipelines/expected.h>

namespace zyppng {

  using RepoInfo            = zypp::RepoInfo;
  using RepoStatus          = zypp::RepoStatus;
  using RepoInfoList        = zypp::RepoInfoList;
  using ServiceInfo         = zypp::ServiceInfo;
  using RepoManagerOptions  = zypp::RepoManagerOptions;
  using PluginRepoverification = zypp_private::repo::PluginRepoverification;

  ZYPP_FWD_DECL_TYPE_WITH_REFS( Context );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( SyncContext );
  ZYPP_FWD_DECL_TYPE_WITH_REFS( ProgressObserver );
  ZYPP_FWD_DECL_TEMPL_TYPE_WITH_REFS_ARG1 ( RepoManager, ZyppContextRefType );

  using SyncRepoManager    = RepoManager<SyncContextRef>;
  using SyncRepoManagerRef = RepoManagerRef<SyncContextRef>;

  using AsyncRepoManager    = RepoManager<ContextRef>;
  using AsyncRepoManagerRef = RepoManagerRef<ContextRef>;

  /** Whether repo is not under RM control and provides its own methadata paths. */
  inline bool isTmpRepo( const RepoInfo & info_r )
  { return( info_r.filepath().empty() && info_r.usesAutoMetadataPaths() ); }

  inline expected<void> assert_alias(const RepoInfo &info)
  {
    if ( info.alias().empty() )
      return expected<void>::error( ZYPP_EXCPT_PTR(zypp::repo::RepoNoAliasException( info ) ) );
    // bnc #473834. Maybe we can match the alias against a regex to define
    // and check for valid aliases
    if ( info.alias()[0] == '.')
      return expected<void>::error( ZYPP_EXCPT_PTR( zypp::repo::RepoInvalidAliasException(
        info, _("Repository alias cannot start with dot."))) );

    return expected<void>::success();
  }

  inline expected<void> assert_alias(const ServiceInfo &info) {
    if (info.alias().empty())
      return expected<void>::error( ZYPP_EXCPT_PTR(zypp::repo::ServiceNoAliasException(info)));
    // bnc #473834. Maybe we can match the alias against a regex to define
    // and check for valid aliases
    if (info.alias()[0] == '.')
      return expected<void>::error( ZYPP_EXCPT_PTR(zypp::repo::ServiceInvalidAliasException(
                   info, _("Service alias cannot start with dot."))));

    return expected<void>::success();
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
  struct RepoCollector : private zypp::base::NonCopyable
  {
    RepoCollector()
    {}

    RepoCollector(std::string  targetDistro_)
      : targetDistro(std::move(targetDistro_))
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
  expected<std::list<RepoInfo>> repositories_in_file( const zypp::Pathname & file );

  ////////////////////////////////////////////////////////////////////////////

  expected<void> assert_urls( const RepoInfo & info );

  inline expected<void> assert_url( const ServiceInfo & info )
  {
    if ( ! info.url().isValid() )
      return expected<void>::error( ZYPP_EXCPT_PTR ( zypp::repo::ServiceNoUrlException( info ) ) );
    return expected<void>::success();
  }

  /**
     * \short Calculates the raw cache path for a repository, this is usually
     * /var/cache/zypp/alias
     */
  inline expected<zypp::Pathname> rawcache_path_for_repoinfo( const RepoManagerOptions &opt, const RepoInfo &info )
  {
    using namespace zyppng::operators;
    return assert_alias(info) | and_then( [&](){ return make_expected_success( isTmpRepo( info ) ? info.metadataPath() : opt.repoRawCachePath / info.escaped_alias()); });
  }

  /**
     * \short Calculates the raw product metadata path for a repository, this is
     * inside the raw cache dir, plus an optional path where the metadata is.
     *
     * It should be different only for repositories that are not in the root of
     * the media.
     * for example /var/cache/zypp/alias/addondir
     */
  inline expected<zypp::Pathname> rawproductdata_path_for_repoinfo( const RepoManagerOptions &opt, const RepoInfo &info )
  {
    using namespace zyppng::operators;
    return rawcache_path_for_repoinfo( opt, info ) | and_then( [&]( zypp::Pathname p ) { return make_expected_success( p / info.path() ); } );
  }

  /**
     * \short Calculates the packages cache path for a repository
     */
  inline expected<zypp::Pathname> packagescache_path_for_repoinfo( const RepoManagerOptions &opt, const RepoInfo &info )
  {
    using namespace zyppng::operators;
    return assert_alias(info) |
    and_then([&](){ return make_expected_success(isTmpRepo( info ) ? info.packagesPath() : opt.repoPackagesCachePath / info.escaped_alias()); });
  }

  /**
     * \short Calculates the solv cache path for a repository
     */
  inline expected<zypp::Pathname> solv_path_for_repoinfo( const RepoManagerOptions &opt, const RepoInfo &info )
  {
    using namespace zyppng::operators;
    return assert_alias(info) |
    and_then([&](){ return make_expected_success(isTmpRepo( info ) ? info.metadataPath().dirname() / "%SLV%" : opt.repoSolvCachePath / info.escaped_alias()); });
  }

  ////////////////////////////////////////////////////////////////////////////

  /** Functor collecting ServiceInfos into a ServiceSet. */
  class ServiceCollector
  {
  public:
    using ServiceSet = std::set<ServiceInfo>;

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
  bool autoPruneInDir( const zypp::Pathname & path_r );

  /*!
   * \brief The RepoManager class
   * Provides knowledge and methods to maintain repo settings and metadata for a given context.
   *
   * Depending on the \a ZyppContextRefType the convenienve functions execute the workflows in
   * a sync or async way. In sync mode libzypp's legacy reports are executed. Otherwise progress report
   * is only provided via the \ref ProgressObserver classes.
   */
  template <typename ZyppContextRefType>
  class RepoManager : public Base, private MaybeAsyncMixin<std::is_same_v<ZyppContextRefType, ContextRef>>
  {
    ZYPP_ADD_PRIVATE_CONSTR_HELPER ();
    ZYPP_ENABLE_MAYBE_ASYNC_MIXIN( (std::is_same_v<ZyppContextRefType, ContextRef>) );

  public:

    using ContextRefType      = ZyppContextRefType;
    using ContextType         = typename ZyppContextRefType::element_type;

    using RawMetadataRefreshPolicy = zypp::RepoManagerFlags::RawMetadataRefreshPolicy;
    using CacheBuildPolicy = zypp::RepoManagerFlags::CacheBuildPolicy;


    using RefreshCheckStatus = zypp::RepoManagerFlags::RefreshCheckStatus;

    /** Flags for tuning RefreshService */
    using RefreshServiceBit     = zypp::RepoManagerFlags::RefreshServiceBit;
    using RefreshServiceOptions = zypp::RepoManagerFlags::RefreshServiceOptions;


    ZYPP_DECL_PRIVATE_CONSTR_ARGS (RepoManager, ZyppContextRefType zyppCtx, RepoManagerOptions opt  );

    template < typename ...Args >
    inline static expected<std::shared_ptr<RepoManager<ZyppContextRefType>>> create ( Args && ...args ) {
      using namespace zyppng::operators;
      auto mgr =  std::make_shared< RepoManager<ZyppContextRefType> >( private_constr_t{}, std::forward<Args>(args)... );
      return mgr->initialize() | and_then( [mgr](){ return make_expected_success(mgr); } );
    }

  public:

    /**
     * Functor thats filter RepoInfo by service which it belongs to.
     */
    struct MatchServiceAlias
    {
    public:
      MatchServiceAlias( std::string  alias_ ) : alias(std::move(alias_)) {}
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


    virtual ~RepoManager();

  public:

    expected<void> initialize ();

    ContextRefType zyppContext() const {
      return _zyppContext;
    }

    const RepoManagerOptions &options() const;

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
    expected<zypp::Pathname> metadataPath( const RepoInfo & info ) const
    { return rawcache_path_for_repoinfo( _options, info ); }

    expected<zypp::Pathname> packagesPath( const RepoInfo & info ) const
    { return packagescache_path_for_repoinfo( _options, info ); }

    static expected<RepoStatus> metadataStatus( const RepoInfo & info, const RepoManagerOptions &options );
    expected<RepoStatus> metadataStatus( const RepoInfo & info ) const;

    expected<void> cleanMetadata( const RepoInfo & info, ProgressObserverRef myProgress = nullptr );

    expected<void> cleanPackages( const RepoInfo & info, ProgressObserverRef myProgress = nullptr , bool isAutoClean = false );

    static zypp::repo::RepoType probeCache( const zypp::Pathname & path_r );

    expected<void> cleanCacheDirGarbage( ProgressObserverRef myProgress = nullptr );

    expected<void> cleanCache( const RepoInfo & info, ProgressObserverRef myProgress = nullptr );

    expected<bool> isCached( const RepoInfo & info ) const
    {
      using namespace zyppng::operators;
      return solv_path_for_repoinfo( _options, info )
          | and_then( [&]( zypp::Pathname solvPath) { return make_expected_success( zypp::PathInfo(solvPath / "solv").isExist()); } );
    }

    expected<RepoStatus> cacheStatus( const RepoInfo & info ) const
    { return cacheStatus( info, _options ); }

    static expected<RepoStatus> cacheStatus( const RepoInfo & info, const RepoManagerOptions &options )
    {
      using namespace zyppng::operators;
      return solv_path_for_repoinfo( options, info )
      | and_then( [&]( zypp::Pathname solvPath) {
        return make_expected_success ( RepoStatus::fromCookieFile(solvPath / "cookie") );
      });
    }

    expected<void> loadFromCache( const RepoInfo & info, ProgressObserverRef myProgress = nullptr );

    expected<RepoInfo> addProbedRepository( RepoInfo info, zypp::repo::RepoType probedType );

    expected<void> removeRepository( const RepoInfo & info, ProgressObserverRef myProgress = nullptr );

    expected<RepoInfo> modifyRepository( const std::string & alias, const RepoInfo & newinfo_r, ProgressObserverRef myProgress = nullptr );

    expected<RepoInfo> getRepositoryInfo( const std::string & alias );
    expected<RepoInfo> getRepositoryInfo( const zypp::Url & url, const zypp::url::ViewOption &urlview );

    expected<RefreshCheckStatus> checkIfToRefreshMetadata( const RepoInfo & info, const std::vector<zypp::Url> &urls, RawMetadataRefreshPolicy policy );

    expected<RefreshCheckStatus> checkIfToRefreshMetadata( const RepoInfo & info, const zypp::Url & url, RawMetadataRefreshPolicy policy ) {
      return checkIfToRefreshMetadata ( info, std::vector<zypp::Url>{url}, policy );
    }

    /**
     * \short Refresh local raw cache
     *
     * Will try to download the metadata
     *
     * In case of failure the metadata remains as it was before.
     *
     * The returned expected can contain one of the following errors:
     * \ref repo::RepoNoUrlException if no urls are available.
     * \ref repo::RepoNoAliasException if can't figure an alias
     * \ref repo::RepoUnknownTypeException if the metadata is unknown
     * \ref repo::RepoException if the repository is invalid
     *         (no valid metadata found at any of baseurls)
     *
     * \todo Currently no progress is generated, especially for the async code
     *       We might need to change this
     */
    expected<void> refreshMetadata( const RepoInfo & info, RawMetadataRefreshPolicy policy, ProgressObserverRef myProgress = nullptr  );

    std::vector<std::pair<RepoInfo, expected<void> > > refreshMetadata(std::vector<RepoInfo> infos, RawMetadataRefreshPolicy policy, ProgressObserverRef myProgress = nullptr  );

    expected<zypp::repo::RepoType> probe( const std::vector<zypp::Url> & url, const zypp::Pathname & path = zypp::Pathname() ) const;

    expected<void> buildCache( const RepoInfo & info, CacheBuildPolicy policy, ProgressObserverRef myProgress = nullptr );

    /*!
     * Adds the repository in \a info and returns the updated \ref RepoInfo object.
     */
    expected<RepoInfo> addRepository( const RepoInfo & info, ProgressObserverRef myProgress = nullptr );

    expected<void> addRepositories( const zypp::Url & url, ProgressObserverRef myProgress = nullptr );

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

    expected<zypp::repo::ServiceType> probeService( const zypp::Url & url ) const;

    expected<void> addService( const ServiceInfo & service );
    expected<void> addService( const std::string & alias, const zypp::Url & url )
    { return addService( ServiceInfo( alias, url ) ); }

    expected<void> removeService( const std::string & alias );
    expected<void> removeService( const ServiceInfo & service )
    { return removeService( service.alias() ); }

    expected<void> refreshService( const std::string & alias, const RefreshServiceOptions & options_r );
    expected<void> refreshService( const ServiceInfo & service, const RefreshServiceOptions & options_r )
    {  return refreshService( service.alias(), options_r ); }

    expected<void> refreshServices( const RefreshServiceOptions & options_r );

    expected<void> modifyService( const std::string & oldAlias, const ServiceInfo & newService );

    static expected<void> touchIndexFile( const RepoInfo & info, const RepoManagerOptions &options );

    expected<void> setCacheStatus( const RepoInfo & info, const RepoStatus & status )
    {
      using namespace zyppng::operators;
      return solv_path_for_repoinfo( _options, info )
      | and_then( [&]( zypp::Pathname base ){
        try {
          zypp::filesystem::assert_dir(base);
          status.saveToCookieFile( base / "cookie" );
        } catch ( const zypp::Exception &e ) {
          return expected<void>::error( std::make_exception_ptr(e) );
        }
        return expected<void>::success();
      });
    }

    /*!
     * Checks for any of the given \a urls if there is no geoip data available, caches the results
     * in the metadata cache for 24hrs. The given urls need to be configured as valid geoIP targets ( usually download.opensuse.org )
     */
    expected<void> refreshGeoIp ( const RepoInfo::url_set &urls );

    template<typename OutputIterator>
    void getRepositoriesInService( const std::string & alias, OutputIterator out ) const
    {
      MatchServiceAlias filter( alias );
      std::copy( boost::make_filter_iterator( filter, repos().begin(), repos().end() ),
        boost::make_filter_iterator( filter, repos().end(), repos().end() ),
        out);
    }

    zypp::Pathname generateNonExistingName( const zypp::Pathname & dir, const std::string & basefilename ) const;

    std::string generateFilename( const RepoInfo & info ) const
    { return filenameFromAlias( info.alias(), "repo" ); }

    std::string generateFilename( const ServiceInfo & info ) const
    { return filenameFromAlias( info.alias(), "service" ); }


  protected:
    expected<void> saveService( ServiceInfo & service ) const;
    expected<void> touchIndexFile( const RepoInfo & info );

  protected:
    expected<void> init_knownServices();
    expected<void> init_knownRepositories();

  public:
    const RepoSet & repos() const { return _reposX; }
    RepoSet & reposManip()        { if ( ! _reposDirty ) _reposDirty = true; return _reposX; }

  public:
    PluginRepoverification pluginRepoverification() const
    { return _pluginRepoverification; }

  protected:
    ContextRefType      _zyppContext;
    RepoManagerOptions	_options;
    RepoSet 		_reposX;
    ServiceSet		_services;
    PluginRepoverification _pluginRepoverification;
    zypp::DefaultIntegral<bool,false> _reposDirty;
  };
}

#endif //ZYPP_NG_REPOMANAGER_INCLUDED
