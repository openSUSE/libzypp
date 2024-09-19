/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ng/ServiceInfo.h
 *
 */
#ifndef ZYPP_NG_SERVICE_INFO_H_INCLUDED
#define ZYPP_NG_SERVICE_INFO_H_INCLUDED

#include <set>
#include <string>

#include <zypp/Url.h>

#include <zypp/base/Iterable.h>
#include <zypp/repo/ServiceType.h>
#include <zypp/Date.h>


#include <zypp/ng/repoinfo.h>
#include <zypp/repo/ServiceRepoState.h>

///////////////////////////////////////////////////////////////////
namespace zyppng
{ /////////////////////////////////////////////////////////////////

  struct ServiceInfoSharedData;

  ///////////////////////////////////////////////////////////////////
  /// \class ServiceInfo
  /// \brief Service data
  ///
  /// \note Name and Url are subject to repo variable replacement
  /// (\see \ref RepoVariablesStringReplacer).
  ///
  class ServiceInfo : public repo::RepoInfoBase
  {
  public:
    /**
     * Default ctor creates \ref noService.
     * \internal
     */
    ServiceInfo( zyppng::ContextBaseRef contextRef );

    /**
     *  Creates ServiceInfo with specified alias.
     *
     * \param alias unique short name of service
     * \internal
     */
    ServiceInfo( zyppng::ContextBaseRef contextRef,  const std::string & alias );

    /**
     * ServiceInfo with alias and its URL
     *
     * \param alias unique shortname of service
     * \param url url to service
     * \internal
     */
    ServiceInfo( zyppng::ContextBaseRef contextRef,  const std::string & alias, const zypp::Url& url );

    ServiceInfo(const ServiceInfo &) = default;
    ServiceInfo(ServiceInfo &&) = default;
    ServiceInfo &operator=(const ServiceInfo &) = default;
    ServiceInfo &operator=(ServiceInfo &&) = default;

    ~ServiceInfo() override;

    /** The service url */
    zypp::Url url() const;

    /** The service raw url (no variables replaced) */
    zypp::Url rawUrl() const;

    /** Set the service url (raw value) */
    void setUrl( const zypp::Url& url );


    /** Service type */
    zypp::repo::ServiceType type() const;

    /** Set service type */
    void setType( const zypp::repo::ServiceType & type );

    /** Lazy init service type */
    void setProbedType( const zypp::repo::ServiceType & t ) const;

    /** \name Housekeeping data
     * You don't want to use the setters unless you are a \ref RepoManager.
     */
    //@{
    /** Sugested TTL between two metadata auto-refreshs.
     * The value (in seconds) may be provided in repoindex.xml:xpath:/repoindex@ttl.
     * Default is \a 0 - perform each auto-refresh request.
     */
    zypp::Date::Duration ttl() const;

    /** Set sugested TTL. */
    void setTtl( zypp::Date::Duration ttl_r );

    /** Lazy init sugested TTL. */
    void setProbedTtl( zypp::Date::Duration ttl_r ) const;

    /** Date of last refresh (if known). */
    zypp::Date lrf() const;

    /** Set date of last refresh. */
    void setLrf( zypp::Date lrf_r );
    //@}
    //
    /** \name Set of repos (repository aliases) to enable on next refresh.
     *
     * Per default new repositories are created in disabled state. But repositories
     * mentioned here will be created in enabled state on the next refresh.
     * Afterwards they get removed from the list.
     */
    //@{
    /** Container of repos. */
    using ReposToEnable = std::set<std::string>;
    bool                          reposToEnableEmpty() const;
    ReposToEnable::size_type      reposToEnableSize() const;
    ReposToEnable::const_iterator reposToEnableBegin() const;
    ReposToEnable::const_iterator reposToEnableEnd() const;
    zypp::Iterable<ReposToEnable::const_iterator> reposToEnable() const
    { return zypp::makeIterable( reposToEnableBegin(), reposToEnableEnd() ); }

    /** Whether \c alias_r is mentioned in ReposToEnable. */
    bool repoToEnableFind( const std::string & alias_r ) const;

    /** Add \c alias_r to the set of ReposToEnable. */
    void addRepoToEnable( const std::string & alias_r );
    /** Remove \c alias_r from the set of ReposToEnable. */
    void delRepoToEnable( const std::string & alias_r );
    /** Clear the set of ReposToEnable. */
    void clearReposToEnable();
    //@}

    /** \name Set of repos (repository aliases) to disable on next refresh.
     *
     * Repositories mentioned here will be disabled on the next refresh, in case they
     * still exist. Afterwards they get removed from the list.
     */
    //@{
    /** Container of repos. */
    using ReposToDisable = std::set<std::string>;
    bool                           reposToDisableEmpty() const;
    ReposToDisable::size_type      reposToDisableSize() const;
    ReposToDisable::const_iterator reposToDisableBegin() const;
    ReposToDisable::const_iterator reposToDisableEnd() const;
    zypp::Iterable<ReposToDisable::const_iterator> reposToDisable() const
    { return zypp::makeIterable( reposToDisableBegin(), reposToDisableEnd() ); }

    /** Whether \c alias_r is mentioned in ReposToDisable. */
    bool repoToDisableFind( const std::string & alias_r ) const;

    /** Add \c alias_r to the set of ReposToDisable. */
    void addRepoToDisable( const std::string & alias_r );
    /** Remove \c alias_r from the set of ReposToDisable. */
    void delRepoToDisable( const std::string & alias_r );
    /** Clear the set of ReposToDisable. */
    void clearReposToDisable();
    //@}

    /** \name The original repo state as defined by the repoindex.xml upon last refresh.
     *
     * This state is remembered to detect any user modifications applied to the repos.
     * It may not be available for all repos or in plugin services. In this case all
     * changes requested by a service refresh are applied unconditionally.
     */
    //@{
    using RepoState  = zypp::ServiceRepoState;
    using RepoStates = std::map<std::string, RepoState>;

    /** Access the remembered repository states. */
    const RepoStates & repoStates() const;

    /** Remember a new set of repository states. */
    void setRepoStates( RepoStates newStates_r );
    //@}

  public:
    /**
     * Writes ServiceInfo to stream in ".service" format
     *
     * \param str stream where serialized version service is written
     */
    std::ostream & dumpAsIniOn( std::ostream & str ) const override;

    /**
     * Write an XML representation of this ServiceInfo object.
     *
     * \param str
     * \param content if not empty, produces <service ...>content</service>
     *                otherwise <service .../>
     */
    std::ostream & dumpAsXmlOn( std::ostream & str, const std::string & content = "" ) const override;

  private:
    ServiceInfoSharedData *pimpl();
    const ServiceInfoSharedData *pimpl() const;
  };
  ///////////////////////////////////////////////////////////////////
  ///
  /** \relates ServiceInfo */
  using ServiceInfoList = std::list<ServiceInfo>;

  /** \relates RepoInfoBase */
  inline bool operator==( const ServiceInfo & lhs, const ServiceInfo & rhs )
  { return lhs.alias() == rhs.alias(); }

  /** \relates RepoInfoBase */
  inline bool operator!=( const ServiceInfo & lhs, const ServiceInfo & rhs )
  { return lhs.alias() != rhs.alias(); }

  inline bool operator<( const ServiceInfo & lhs, const ServiceInfo & rhs )
  { return lhs.alias() < rhs.alias(); }

  /** \relates ServiceInfo Stream output */
  std::ostream & operator<<( std::ostream & str, const ServiceInfo & obj );


    /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP_NG_SERVICE_INFO_H_INCLUDED
