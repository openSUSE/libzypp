/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/ServiceInfo.h
 *
 */
#ifndef ZYPP_SERVICE_H
#define ZYPP_SERVICE_H

#include <set>
#include <string>

#include <zypp/Url.h>

#include <zypp/base/Iterable.h>
#include <zypp/repo/ServiceType.h>
#include <zypp/repo/ServiceRepoState.h>
#include <zypp/RepoInfo.h>
#include <zypp/Date.h>

namespace zyppng {
  class ServiceInfo;
}

ZYPP_BEGIN_LEGACY_API

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  /// \class ServiceInfo
  /// \brief Service data
  ///
  /// \note Name and Url are subject to repo variable replacement
  /// (\see \ref RepoVariablesStringReplacer).
  ///
  class ZYPP_API ZYPP_INTERNAL_DEPRECATE ServiceInfo : public repo::RepoInfoBase
  {
  public:
    /** Default ctor creates \ref noService.*/
    ServiceInfo();

    ServiceInfo( const zyppng::ServiceInfo &i );

    ServiceInfo(const ServiceInfo &other);
    ServiceInfo(ServiceInfo &&other);
    ServiceInfo &operator=(const ServiceInfo &other);
    ServiceInfo &operator=(ServiceInfo &&other);

    /**
     *  Creates ServiceInfo with specified alias.
     *
     * \param alias unique short name of service
     */
    ServiceInfo( const std::string & alias );

    /**
     * ServiceInfo with alias and its URL
     *
     * \param alias unique shortname of service
     * \param url url to service
     */
    ServiceInfo( const std::string & alias, const Url& url );

    ~ServiceInfo() override;

  public:
    /** Represents an empty service. */
    static const ServiceInfo noService ZYPP_API;

  public:

    /** The service url */
    Url url() const;

    /** The service raw url (no variables replaced) */
    Url rawUrl() const;

    /** Set the service url (raw value) */
    void setUrl( const Url& url );


    /** Service type */
    repo::ServiceType type() const;

    /** Set service type */
    void setType( const repo::ServiceType & type );

    /** Lazy init service type */
    void setProbedType( const repo::ServiceType & t ) const;

    /** \name Housekeeping data
     * You don't want to use the setters unless you are a \ref RepoManager.
     */
    //@{
    /** Sugested TTL between two metadata auto-refreshs.
     * The value (in seconds) may be provided in repoindex.xml:xpath:/repoindex@ttl.
     * Default is \a 0 - perform each auto-refresh request.
     */
    Date::Duration ttl() const;

    /** Set sugested TTL. */
    void setTtl( Date::Duration ttl_r );

    /** Lazy init sugested TTL. */
    void setProbedTtl( Date::Duration ttl_r ) const;

    /** Date of last refresh (if known). */
    Date lrf() const;

    /** Set date of last refresh. */
    void setLrf( Date lrf_r );
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
    Iterable<ReposToEnable::const_iterator> reposToEnable() const
    { return makeIterable( reposToEnableBegin(), reposToEnableEnd() ); }

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
    Iterable<ReposToDisable::const_iterator> reposToDisable() const
    { return makeIterable( reposToDisableBegin(), reposToDisableEnd() ); }

    /** Whether \c alias_r is mentioned in ReposToDisable. */
    bool repoToDisableFind( const std::string & alias_r ) const;

    /** Add \c alias_r to the set of ReposToDisable. */
    void addRepoToDisable( const std::string & alias_r );
    /** Remove \c alias_r from the set of ReposToDisable. */
    void delRepoToDisable( const std::string & alias_r );
    /** Clear the set of ReposToDisable. */
    void clearReposToDisable();
    //@}

    using RepoState  = ServiceRepoState;
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


    zyppng::ServiceInfo &ngServiceInfo();
    const zyppng::ServiceInfo &ngServiceInfo() const;


  private:
    zyppng::repo::RepoInfoBase &pimpl() override;
    const zyppng::repo::RepoInfoBase &pimpl() const override;

    std::unique_ptr<zyppng::ServiceInfo> _pimpl;
    bool _ownsPimpl = true;
  };
  ///////////////////////////////////////////////////////////////////

  /** \relates ServiceInfo */
  using ServiceInfo_Ptr = shared_ptr<ServiceInfo>;
  /** \relates ServiceInfo */
  using ServiceInfo_constPtr = shared_ptr<const ServiceInfo>;
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
  std::ostream & operator<<( std::ostream & str, const ServiceInfo & obj ) ZYPP_API;


    /////////////////////////////////////////////////////////////////
} // namespace zypp

ZYPP_END_LEGACY_API

#endif // ZYPP_SERVICE_H
