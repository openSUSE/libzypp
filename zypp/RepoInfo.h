/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file	zypp/RepoInfo.h
 *
*/
#ifndef ZYPP2_REPOSITORYINFO_H
#define ZYPP2_REPOSITORYINFO_H

#include <list>
#include <set>

#include <zypp-core/base/Iterator.h>
#include <zypp-core/Globals.h>
#include <zypp-core/MirroredOrigin.h>
#include <zypp-core/TriBool.h>
#include <zypp-core/Url.h>

#include <zypp/Locale.h>
#include <zypp/repo/RepoType.h>
#include <zypp/repo/RepoVariables.h>
#include <zypp/repo/RepoInfoBase.h>

///////////////////////////////////////////////////////////////////
namespace zypp
{ /////////////////////////////////////////////////////////////////

  ///////////////////////////////////////////////////////////////////
  //
  //	CLASS NAME : RepoInfo
  //
  /**
   * \short What is known about a repository
   *
   * The class RepoInfo represents everything that
   * is known about a software repository.
   *
   * It can be used to store information about known
   * sources.
   *
   * This class tries to be compatible with the
   * concept of a .repo file used by YUM and
   * also available in the openSUSE build service.
   * See <tt>man yum.conf</tt>.
   *
   * Example file
   *
   * \code
   * [ruby]
   * name=Ruby repository (openSUSE_10.2)
   * type=rpm-md
   * baseurl=http://software.opensuse.org/download/ruby/openSUSE_10.2/
   *         http://some.opensuse.mirror/ruby/openSUSE_10.2/
   * gpgcheck=1
   * gpgkey=http://software.opensuse.org/openSUSE-Build-Service.asc
   * enabled=1
   * priority=10
   * \endcode
   *
   * \note A RepoInfo is a hint about how
   * to create a Repository.
   *
   * \note Name, baseUrls and mirrorUrl are subject to repo variable replacement
   * (\see \ref RepoVariablesStringReplacer).
   */
  class ZYPP_API RepoInfo : public repo::RepoInfoBase
  {
    friend std::ostream & operator<<( std::ostream & str, const RepoInfo & obj );

    public:
      RepoInfo();
      ~RepoInfo() override;

      RepoInfo(const RepoInfo &) = default;
      RepoInfo(RepoInfo &&) = default;
      RepoInfo &operator=(const RepoInfo &) = default;
      RepoInfo &operator=(RepoInfo &&) = default;

      /** Represents no Repository (one with an empty alias). */
      static const RepoInfo noRepo;

    public:
      /**
       * The default priority (\c 99).
       */
      static unsigned defaultPriority();
      /**
       * The least priority (<tt>unsigned(-1)</tt>).
       */
      static unsigned noPriority();
      /**
       * Repository priority for solver.
       * Some number between \c 1 (highest priority) and \c 99 (\ref defaultPriority).
       */
      unsigned priority() const;
      /**
       * Set repository priority for solver.
       * A \c newval_r of \c 0 sets the default priority.
       * \see \ref priority.
       */
      void setPriority( unsigned newval_r );

      using url_set = std::list<Url>;
      using urls_size_type = url_set::size_type;
      using urls_const_iterator = transform_iterator<repo::RepoVariablesUrlReplacer, url_set::const_iterator>;
      /**
       * whether repository urls are available
       */
      bool baseUrlsEmpty() const;
      /**
       * Whether there are manualy configured repository urls.
       * If \c false, a mirrorlist might be used.
       */
      bool baseUrlSet() const;
      /**
       * number of repository urls
       */
      urls_size_type baseUrlsSize() const;
      /**
       * iterator that points at begin of repository urls
       */
      urls_const_iterator baseUrlsBegin() const;
      /**
       * iterator that points at end of repository urls
       */
      urls_const_iterator baseUrlsEnd() const;

      /**
       * Pars pro toto: The first repository url, this is either baseUrls().front()
       * or if no baseUrl is defined the first available mirror.
       */
      Url url() const;
      /**
       * Pars pro toto: The first repository raw url (no variables replaced) this is either rawBaseUrls().front()
       * or if no baseUrl is defined the first available mirror.
       */
      Url rawUrl() const;

      /**
       * Returns the location URL for the repository, this is either the first configured baseUrl
       * or a configured mirror or metalink URL.
       * Use this if you need to display a repo URL to the user.
       */
      Url location() const;

      /**
       * The complete set of repository urls as configured
       */
      url_set baseUrls() const;
      /**
       * The complete set of raw repository urls (no variables replaced)
       */
      url_set rawBaseUrls() const;

      /**
       * Add a base url. \see baseUrls
       * \param url The base url for the repository.
       *
       * To recreate the base URLs list, use \ref setBaseUrl(const Url &) followed
       * by addBaseUrl().
       */
      void addBaseUrl( Url url );
      /**
       * Clears current base URL list and adds \a url.
       */
      void setBaseUrl( Url url );
      /**
       * Clears current base URL list and adds an \ref url_set.
       */
      void setBaseUrls( url_set urls );

      /**
       * The repodata origins
       *
       * These are either the configured baseurls including the downloaded
       * mirror list (\see \ref mirrorListUrl) or meta link urls.
       *
       * Also if there is just one baseUrl pointing to a well known opensuse server
       * it might contain additional mirrors requested from the server on demand
       *
       */
      MirroredOriginSet repoOrigins() const;

      /**
       * whether repo origins are available
       */
      bool repoOriginsEmpty() const;

      /**
       * \short Repository path
       *
       * Pathname relative to the base Url where the product/repository
       * is located
       *
       * For media containing more than one product, or repositories not
       * located at the root of the media it is important to know the path
       * to the product directory relative to the media root. So a media
       * verifier can be set for that media. You may also read it as
       * <tt>baseUrl = url to mount</tt> and <tt>path = path on the
       * mounted media</tt>.
       *
       * It is not mandatory, and the default is \c /.
       *
       * \note As a repository can have multiple Urls, the path is unique and
       * the same for all Urls, so it is assumed all the Urls have the
       * same media layout.
       *
       */
      Pathname path() const;
      /**
       * set the product path. \see path()
       * \param path the path to the product
       */
      void setPath( const Pathname &path );

      /**
       * Url of a file which contains a list of repository urls.
       * This is either the configured mirrorlist or metalink Url.
       */
      Url mirrorListUrl() const;
      /**
       * The raw \ref mirrorListUrl (no variables replaced).
       * This is either the configured mirrorlist or metalink Url.
       */
      Url rawMirrorListUrl() const;

      /** Set the raw mirrorlist url. */
      void setMirrorlistUrl( const Url &url );
      /** Set the raw metalink url. */
      void setMetalinkUrl( const Url &url );

      /** The configured raw mirrorlist url. */
      Url rawCfgMirrorlistUrl() const;
      /** The configured raw metalink url. */
      Url rawCfgMetalinkUrl() const;

#if LEGACY(1735)
      /** \deprecated Use \ref setMirrorlistUrl (lowercase 'l').
       * mirrorListUrl (uppercase 'L') is a computed value,
       * either the configured mirrorlist or metalink Url.
       */
      void setMirrorListUrl( const Url &url ) ZYPP_DEPRECATED;
      /** \deprecated Only one Url. */
      void setMetalinkUrls( url_set urls ) ZYPP_DEPRECATED;
      /** \deprecated Only one Url. */
      void setMirrorListUrls( url_set urls ) ZYPP_DEPRECATED;
#endif

      /**
       * Type of repository,
       *
       */
      repo::RepoType type() const;
      /**
       * This allows to adjust the \ref  RepoType lazy, from \c NONE to
       * some probed value, even for const objects.
       *
       * This is a NOOP if the current type is not \c NONE.
       */
      void setProbedType( const repo::RepoType &t ) const;
      /**
       * set the repository type \see type
       * \param t
       */
      void setType( const repo::RepoType &t );

      /**
       * \short Path where this repo metadata was read from
       *
       * \note could be an empty pathname for repo
       * infos created in memory.
       */
      Pathname metadataPath() const;
      /**
       * \short Set the path where the local metadata is stored
       *
       * The path to the repositories metadata is usually provided by
       * the RepoManager. If you want to use a temporary repository
       * (not under RepoManagers control), and you set a metadataPath
       * with basename \c %AUTO%, all data directories (raw metadata,
       * solv file and package cache) will be created by replacing \c %AUTO%
       * with \c %RAW%, \c %SLV% or \c %PKG% . This will change the value
       * of \ref packagesPath accordingly, unless you assigned a custom
       * value using \ref setPackagesPath.
       *
       * \code
       *   RepoInfo repo;
       *   repo.setAlias( "Temp" );
       *   repo.setBaseUrl( Url("http://someserver/somepath/") );
       *   repo.setMetadataPath( "/tmp/temprepodata/%AUTO%" );
       *
       *   // will use
       *   //  /tmp/temprepodata/%RAW% - raw metadata
       *   //                   /%SLV% - solv file
       *   //                   /%PKG% - packages
       *\endcode
       *
       * \param path directory path
       */
      void setMetadataPath( const Pathname &path );

      /** Whether \ref metadataPath uses \c %AUTO% setup. */
      bool usesAutoMetadataPaths() const;

      /**
       * \short Path where this repo packages are cached
       */
      Pathname packagesPath() const;
      /**
       * \short set the path where the local packages are stored
       *
       * \param path directory path
       */
      void setPackagesPath( const Pathname &path );

      /**
       * \short Path where this repo packages are predownloaded
       */
      Pathname predownloadPath() const;


      /** \name Repository gpgchecks
       * How signature checking should be performed for this repo.
       *
       * The values are computed based in the settings of \c gpgcheck, \c repo_gpgcheck
       * end \c pkg_gpgcheck in \c zypp.conf. Explicitly setting these values in the
       * repositories \a .repo file will overwrite the defaults from \c zypp.conf for this
       * repo.
       *
       * If \c gpgcheck is \c on (the default) we will check the signature of repo metadata
       * (packages are secured via checksum inside the metadata). Using unsigned repos
       * needs to be confirmed.
       * Packages from signed repos are accepted if their checksum matches the checksum
       * stated in the repo metadata.
       * Packages from unsigned repos need a valid gpg signature, using unsigned packages
       * needs to be confirmed.
       *
       * The above default behavior can be tuned by explicitly setting \c repo_gpgcheck
       * and/or \c pkg_gpgcheck:
       *
       *   \c repo_gpgcheck = \c on same as the default.
       *
       *   \c repo_gpgcheck = \c off will silently accept unsigned repos. It will NOT turn of
       *   signature checking on the whole. Nevertheless, it's not a secure setting.
       *
       *   \c pkg_gpgcheck = \c on will enforce the package signature checking and the need
       *   to confirm unsigned packages for all repos (signed and unsigned).
       *
       *   \c pkg_gpgcheck = \c off will silently accept unsigned packages. It will NOT turn of
       *   signature checking on the whole. Nevertheless, it's not a secure setting.
       *
       * If \c gpgCheck is \c off (not recommneded), no checks are performed. You can still
       * enable them individually by setting \c repo_gpgcheck and/or \c pkg_gpgcheck to \c on.
       *
       * \code
       *  R: check repo signature is mandatory, confirm unsigned repos
       *  r: check repo signature, unsigned repos are ok but enforce p
       *   : do not check repo signatures
       *
       *  P: check package signature always, confirm unsigned packages
       *  p: like P for unsigned repos, accepted by checksum for signed repos
       *  b: like p but accept unsigned packages
       *   : do not check package signatures
       *                    pkg_
       * gpgcheck 1|     *       0       1
       * ------------------------------------
       * repo_   *1|     R/p     R/b     R/P
       *          0|     r/p     r/b     r/P
       *
       *                    pkg_
       * gpgcheck 0|     *       0       1
       * ------------------------------------
       * repo_   *0|                       P
       *          1|     R       R       R/P
       * \endcode
       */
      //@{
      /** Whether default signature checking should be performed. */
      bool gpgCheck() const;
      /** Set the value for \ref gpgCheck (or \c indeterminate to use the default). */
      void setGpgCheck( TriBool value_r );
      /** \overload \deprecated legacy and for squid */
      void setGpgCheck( bool value_r );

      /** Whether the signature of repo metadata should be checked for this repo. */
      bool repoGpgCheck() const;
      /** Mandatory check (\ref repoGpgCheck is \c on) must ask to confirm using unsigned repos. */
      bool repoGpgCheckIsMandatory() const;
      /** Set the value for \ref repoGpgCheck (or \c indeterminate to use the default). */
      void setRepoGpgCheck( TriBool value_r );

      /** Whether the signature of rpm packages should be checked for this repo. */
      bool pkgGpgCheck() const;
      /** Mandatory check (\ref pkgGpgCheck is not \c off) must ask to confirm using unsigned packages. */
      bool pkgGpgCheckIsMandatory() const;
      /** Set the value for \ref pkgGpgCheck (or \c indeterminate to use the default). */
      void setPkgGpgCheck( TriBool value_r );

      /** Whether the repo metadata are signed and successfully validated or \c indeterminate if unsigned.
       * The value is usually set by \ref repo::Downloader when retrieving the metadata.
       */
      TriBool validRepoSignature() const;
      /** Set the value for \ref validRepoSignature (or \c indeterminate if unsigned). */
      void setValidRepoSignature( TriBool value_r );

      /** Some predefined settings */
      enum class GpgCheck {
        indeterminate,		//< not specified
        On,			//< 1** --gpgcheck
        Strict,			//< 111 --gpgcheck-strict
        AllowUnsigned,		//< 100 --gpgcheck-allow-unsigned
        AllowUnsignedRepo,	//< 10* --gpgcheck-allow-unsigned-repo
        AllowUnsignedPackage,	//< 1*0 --gpgcheck-allow-unsigned-package
        Default,		//< *** --default-gpgcheck
        Off,			//< 0** --no-gpgcheck
      };

      /** Adjust *GpgCheck settings according to \a mode_r.
       * \c GpgCheck::indeterminate will leave the settings as they are.
       * \return whether setting were changed
       */
      bool setGpgCheck( GpgCheck mode_r );
      //@}


      /** Whether gpgkey URLs are defined */
      bool gpgKeyUrlsEmpty() const;
      /** Number of gpgkey URLs defined */
      urls_size_type gpgKeyUrlsSize() const;

      /** The list of gpgkey URLs defined for this repo */
      url_set gpgKeyUrls() const;
      /** The list of raw gpgkey URLs defined for this repo (no variables replaced) */
      url_set rawGpgKeyUrls() const;
      /** Set a list of gpgkey URLs defined for this repo */
      void setGpgKeyUrls( url_set urls );

      /** (leagcy API) The 1st gpgkey URL defined for this repo */
      Url gpgKeyUrl() const;
      /** (leagcy API) The 1st raw gpgkey URL defined for this repo (no variables replaced) */
      Url rawGpgKeyUrl() const;
      /** (leagcy API) Set the gpgkey URL defined for this repo */
      void setGpgKeyUrl( const Url &gpgkey );

      /**
       * \short Whether packages downloaded from this repository will be kept in local cache
       */
      bool keepPackages() const;
      /**
       * \short Set if packaqes downloaded from this repository will be kept in local cache
       *
       * If the setting is true, all downloaded packages from this repository will be
       * copied to the local raw cache.
       *
       * \param keep true (keep the downloaded packages) or false (delete them after installation)
       *
       */
      void setKeepPackages( bool keep );

      /** \ref keepPackages unless the package cache itself enforces keeping the packages.
       * This can be done by creating a file named \c .keep_packages in the package
       * cache's toplevel directory (e.g. /var/cache/zypp/packages/.keep_packages)
       */
      bool effectiveKeepPackages() const;

      /**
       * Gets name of the service to which this repository belongs or empty string
       * if it has been added manually.
       */
      std::string service() const;
      /**
       * sets service which added this repository
       */
      void setService( const std::string& name );

      /**
       * Distribution for which is this repository meant.
       */
      std::string targetDistribution() const;
      /**
       * Sets the distribution for which is this repository meant. This is
       * an in-memory value only, does not get written to the .repo file upon
       * saving.
       */
      void setTargetDistribution(const std::string & targetDistribution);


      /** Content keywords defined. */
      const std::set<std::string> & contentKeywords() const;

      /** Add content keywords */
      void addContent( const std::string & keyword_r );
      /** \overload add keywords from container */
      template <class TIterator>
      void addContentFrom( TIterator begin_r, TIterator end_r )
      { for_( it, begin_r, end_r ) addContent( *it ); }
      /** \overload  */
      template <class TContainer>
      void addContentFrom( const TContainer & container_r )
      { addContentFrom( container_r.begin(), container_r.end() ); }

      /** Check for content keywords.
       * They may be missing due to missing metadata in disabled repos.
       */
      bool hasContent() const;
      /** \overload check for a keywords being present */
      bool hasContent( const std::string & keyword_r ) const;
      /** \overload check for \b all keywords being present */
      template <class TIterator>
      bool hasContentAll( TIterator begin_r, TIterator end_r ) const
      { for_( it, begin_r, end_r ) if ( ! hasContent( *it ) ) return false; return true; }
      /** \overload  */
      template <class TContainer>
      bool hasContentAll( const TContainer & container_r ) const
      { return hasContentAll( container_r.begin(), container_r.end() ); }
      /** \overload check for \b any keyword being present */
      template <class TIterator>
      bool hasContentAny( TIterator begin_r, TIterator end_r ) const
      { for_( it, begin_r, end_r ) if ( hasContent( *it ) ) return true; return false; }
      /** \overload  */
      template <class TContainer>
      bool hasContentAny( const TContainer & container_r ) const
      { return hasContentAny( container_r.begin(), container_r.end() ); }

    public:
      /** \name Repository/Product license
       * In case a repository provides multiple license tarballs in repomd.xml
       * \code
       *   <data type="license">...</data>
       *   <data type="license-sles">...</data>
       *   <data type="license-sled">...</data>
       * \endcode
       * you can address the individual licenses by passing their name
       * (e.g. \c "sles" to access the \c type="license-sles").
       * No on an empty name will refer to \c type="license".
       */
      //@{
      /** Whether there is a license associated with the repo. */
      bool hasLicense() const;
      /** \overload taking a (product)name */
      bool hasLicense( const std::string & name_r ) const;

      /** Whether the repo license has to be accepted, e.g. there is no
       * no acceptance needed for openSUSE.
       */
      bool needToAcceptLicense() const;
      /** \overload taking a (product)name */
      bool needToAcceptLicense( const std::string & name_r ) const;

      /** Return the best license for the current (or a specified) locale. */
      std::string getLicense( const Locale & lang_r = Locale() ) const;
      /** \overload not const LEGACY API */
      std::string getLicense( const Locale & lang_r = Locale() ); // LEGACY API
      /** \overload taking a (product)name */
      std::string getLicense( const std::string & name_r, const Locale & lang_r = Locale() ) const;

      /** Return the locales the license is available for.
       * \ref Locale::noCode is included in case of \c license.txt which does
       * not specify a specific locale.
       */
      LocaleSet getLicenseLocales() const;
      /** \overload taking a (product)name */
      LocaleSet getLicenseLocales( const std::string & name_r ) const;
     //@}

      /**
       * Returns true if this repository requires the media.1/media file to be included
       * in the metadata status and repo status calculations.
       */
      bool requireStatusWithMediaFile () const;

    public:
      /**
       * Write a human-readable representation of this RepoInfo object
       * into the \a str stream. Useful for logging.
       */
      std::ostream & dumpOn( std::ostream & str ) const override;

      /**
       * Write this RepoInfo object into \a str in a <tr>.repo</tt> file format.
       * Raw values, no variable replacement.
       */
      std::ostream & dumpAsIniOn( std::ostream & str ) const override;

      /**
       * Write an XML representation of this RepoInfo object.
       * Repo variables replaced.
       *
       * \param str
       * \param content this argument is ignored (used in other classed derived
       *                from RepoInfoBase.
       */
      std::ostream & dumpAsXmlOn( std::ostream & str, const std::string & content = "" ) const override;

      /** Raw values for RepoManager
       *  \internal
       */
      void getRawGpgChecks( TriBool & g_r, TriBool & r_r, TriBool & p_r ) const;

      /** A string value to track changes requiring a refresh.
       * The strings value is arbitrary but it should change if the
       * repositories URL changes.
       * \internal
       */
      std::string repoStatusString() const;

      struct Impl;
    private:
      friend class RepoManager;

      /** Pointer to implementation */
      RWCOW_pointer<Impl> _pimpl;
  };
  ///////////////////////////////////////////////////////////////////

  /** \relates RepoInfo */
  using RepoInfo_Ptr = shared_ptr<RepoInfo>;
  /** \relates RepoInfo */
  using RepoInfo_constPtr = shared_ptr<const RepoInfo>;
  /** \relates RepoInfo */
  using RepoInfoList = std::list<RepoInfo>;

  /** \relates RepoInfo Stream output */
  std::ostream & operator<<( std::ostream & str, const RepoInfo & obj ) ZYPP_API;

  /** \relates RepoInfo::GpgCheck Stream output */
  std::ostream & operator<<( std::ostream & str, const RepoInfo::GpgCheck & obj ) ZYPP_API;

  /////////////////////////////////////////////////////////////////
} // namespace zypp
///////////////////////////////////////////////////////////////////
#endif // ZYPP2_REPOSITORYINFO_H
