/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaNetworkCommonHandler.cc
 *
*/

#include "MediaNetworkCommonHandler.h"
#include <zypp-core/ng/pipelines/transform.h>

#include <zypp/ZConfig.h>
#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/base/Regex.h>
#include <zypp-core/base/Logger.h>
#include <zypp-core/base/Gettext.h>
#include <zypp/Target.h>
#include <zypp/media/UrlResolverPlugin.h>
#include <zypp-media/auth/CredentialManager>
#include <zypp-curl/auth/CurlAuthData>
#include <zypp-curl/private/curlhelper_p.h>

#include <zypp/ZYppCallbacks.h>

#include <fstream>
#include <curl/curl.h>

using std::endl;

namespace zypp::media
{
  MediaNetworkCommonHandler::MediaNetworkCommonHandler( const MirroredOrigin &origin_r, const Pathname &attach_point_r, const Pathname &urlpath_below_attachpoint_r, const bool does_download_r)
    : MediaHandler( origin_r, attach_point_r, urlpath_below_attachpoint_r, does_download_r )
  {
    _redirTargets.clear ();
    std::transform ( _origin.begin(), _origin.end(), std::back_inserter(_redirTargets), []( const OriginEndpoint &url_r ) { return findGeoIPRedirect(url_r.url()); } );
  }

  void MediaNetworkCommonHandler::attachTo (bool next)
  {
    if ( next )
      ZYPP_THROW(MediaNotSupportedException(url()));

    if( !isUseableAttachPoint( attachPoint() ) ) {
      setAttachPoint( createAttachPoint(), true );
    }

    disconnectFrom(); // clean state if needed

    // here : setup TransferSettings
    setupTransferSettings();

    // FIXME: need a derived class to propelly compare url's
    MediaSourceRef media( new MediaSource( url().getScheme(), url().asString()) );
    setMediaSource(media);
  }

  bool MediaNetworkCommonHandler::checkAttachPoint(const Pathname &apoint) const
  {
    return MediaHandler::checkAttachPoint( apoint, true, true);
  }

  Url MediaNetworkCommonHandler::clearQueryString(const Url &url) const
  {
    return ::internal::clearQueryString(url);
  }

  zypp::Url MediaNetworkCommonHandler::findGeoIPRedirect ( const zypp::Url &url )
  {
    try {
      const auto &conf = ZConfig::instance();
      if ( !conf.geoipEnabled() ) {
        MIL << "GeoIp rewrites disabled via ZConfig." << std::endl;
        return Url();
      }

      if ( !( url.getQueryParam("COUNTRY").empty() && url.getQueryParam("AVOID_COUNTRY").empty() )) {
        MIL << "GeoIp rewrites disabled since the baseurl " << url << " uses an explicit country setting." << std::endl;
        return Url();
      }

      const auto &hostname = url.getHost();
      auto geoipFile = conf.geoipCachePath() / hostname ;
      if ( PathInfo( geoipFile ).isFile() ) {

        MIL << "Found GeoIP file for host: " << hostname << std::endl;

        std::ifstream in( geoipFile.asString() );
        if (!in.is_open()) {
          MIL << "Failed to open GeoIP for host: " << hostname << std::endl;
          return Url();
        }

        try {
          std::string newHost;
          in >> newHost;

          Url newUrl = url;
          newUrl.setHost( newHost );

          MIL << "Found GeoIP rewrite: " << hostname << " -> " << newHost << std::endl;

          return newUrl;

        } catch ( const zypp::Exception &e ) {
          ZYPP_CAUGHT(e);
          MIL << "No valid GeoIP rewrite target found for " << url << std::endl;
        }
      }
    } catch ( const zypp::Exception &e ) {
      ZYPP_CAUGHT(e);
      MIL << "Failed to query GeoIP data, url rewriting disabled." << std::endl;
    }

    // no rewrite
    return Url();
  }

  ///////////////////////////////////////////////////////////////////

  void MediaNetworkCommonHandler::getFile( const OnMediaLocation &file ) const
  {
      // Use absolute file name to prevent access of files outside of the
      // hierarchy below the attach point.
      getFileCopy( file, localPath(file.filename()).absolutename() );
  }

  void MediaNetworkCommonHandler::getDir( const Pathname & dirname, bool recurse_r ) const
  {
    filesystem::DirContent content;
    getDirInfo( content, dirname, /*dots*/false );

    for ( filesystem::DirContent::const_iterator it = content.begin(); it != content.end(); ++it ) {
        Pathname filename = dirname + it->name;
        int res = 0;

        switch ( it->type ) {
        case filesystem::FT_NOT_AVAIL: // old directory.yast contains no typeinfo at all
        case filesystem::FT_FILE:
          getFile( OnMediaLocation( filename ) );
          break;
        case filesystem::FT_DIR: // newer directory.yast contain at least directory info
          if ( recurse_r ) {
            getDir( filename, recurse_r );
          } else {
            res = assert_dir( localPath( filename ) );
            if ( res ) {
              WAR << "Ignore error (" << res <<  ") on creating local directory '" << localPath( filename ) << "'" << endl;
            }
          }
          break;
        default:
          // don't provide devices, sockets, etc.
          break;
        }
    }
  }

  void MediaNetworkCommonHandler::getDirInfo( std::list<std::string> & retlist,
                                 const Pathname & dirname, bool dots ) const
  {
    getDirectoryYast( retlist, dirname, dots );
  }

  void MediaNetworkCommonHandler::getDirInfo( filesystem::DirContent & retlist,
                              const Pathname & dirname, bool dots ) const
  {
    getDirectoryYast( retlist, dirname, dots );
  }

  const char *MediaNetworkCommonHandler::anonymousIdHeader()
  {
    // we need to add the release and identifier to the
    // agent string.
    // The target could be not initialized, and then this information
    // is guessed.
    // bsc#1212187: HTTP/2 RFC 9113 forbids fields ending with a space
    static const std::string _value( str::trim( str::form(
                                                  "X-ZYpp-AnonymousId: %s",
                                                  Target::anonymousUniqueId( Pathname()/*guess root*/ ).c_str()
                                                  )));
    return _value.c_str();
  }

  const char *MediaNetworkCommonHandler::distributionFlavorHeader()
  {
    // we need to add the release and identifier to the
    // agent string.
    // The target could be not initialized, and then this information
    // is guessed.
    // bsc#1212187: HTTP/2 RFC 9113 forbids fields ending with a space
    static const std::string _value( str::trim( str::form(
                                                  "X-ZYpp-DistributionFlavor: %s",
                                                  Target::distributionFlavor( Pathname()/*guess root*/ ).c_str()
                                                  )));
    return _value.c_str();
  }

  Url MediaNetworkCommonHandler::getFileUrl( int mirrorIdx, const Pathname & filename_r ) const
  {
    static const zypp::str::regex invalidRewrites("^.*\\/repomd.xml(.asc|.key)?$|^\\/geoip$");

    if ( mirrorIdx < 0 || mirrorIdx > _redirTargets.size() )
      return {};

    const bool canRedir = _redirTargets[mirrorIdx].isValid() && !invalidRewrites.matches(filename_r.asString());
    const auto &baseUrl = ( canRedir ) ? _redirTargets[mirrorIdx] : _origin[mirrorIdx].url();

    if ( canRedir )
      MIL << "Redirecting " << filename_r << " request to geoip location." << std::endl;

    // Simply extend the URLs pathname:
    Url newurl { baseUrl };
    newurl.appendPathName( filename_r );
    return newurl;
  }

  const char *MediaNetworkCommonHandler::agentString() {
    // we need to add the release and identifier to the
    // agent string.
    // The target could be not initialized, and then this information
    // is guessed.
    // bsc#1212187: HTTP/2 RFC 9113 forbids fields ending with a space
    static const std::string _value(str::trim(str::form(
                                                "ZYpp " LIBZYPP_VERSION_STRING " (curl %s) %s",
                                                curl_version_info(CURLVERSION_NOW)->version,
                                                Target::targetDistribution(Pathname() /*guess root*/).c_str())));
    return _value.c_str();
  }

  void MediaNetworkCommonHandler::setupTransferSettings()
  {
    // fill some settings from url query parameters
    try
    {
      clearTransferSettings();

      CredentialManager cm(CredManagerOptions(ZConfig::instance().repoManagerRoot()));
      for( OriginEndpoint &u : _origin ) {

        u.setConfig( MIRR_SETTINGS_KEY.data() , std::make_any<TransferSettings>()); // init or reset to default

        if ( !u.isValid() )
          ZYPP_THROW(MediaBadUrlException(u.url()));

        TransferSettings &set = u.getConfig<TransferSettings>( MIRR_SETTINGS_KEY.data() );

        checkProtocol(u.url());

        set.setUserAgentString(agentString());

        // apply MediaUrl settings
        if ( u.hasConfig ("http-headers") ) {
          // Set up the handler
          for ( const auto & el : u.getConfig<UrlResolverPlugin::HeaderList>("http-headers") ) {
            std::string header { el.first };
            header += ": ";
            header += el.second;
            MIL << "Added custom header -> " << header << std::endl;
            set.addHeader( std::move(header) );
          }
        }

        // add custom headers for download.opensuse.org (bsc#955801)
        if ( u.url().getHost() == "download.opensuse.org" )
        {
          set.addHeader(anonymousIdHeader());
          set.addHeader(distributionFlavorHeader());
        }
        set.addHeader("Pragma:");

        // apply legacy Url encoded settings
        ::internal::fillSettingsFromUrl( u.url(), set);

        // if the proxy was not set (or explicitly unset) by url, then look...
        if ( set.proxy().empty() )
        {
          // ...at the system proxy settings
          ::internal::fillSettingsSystemProxy( u.url(), set);
        }

        /* Fixes bsc#1174011 "auth=basic ignored in some cases"
         * We should proactively add the password to the request if basic auth is configured
         * and a password is available in the credentials but not in the URL.
         *
         * We will be a bit paranoid here and require that the URL has a user embedded, otherwise we go the default route
         * and ask the server first about the auth method
         */
        if ( set.authType() == "basic"
             && set.username().size()
             && !set.password().size() ) {
          const auto cred = cm.getCred( u.url() );
          if ( cred && cred->valid() ) {
            if ( !set.username().size() )
              set.setUsername(cred->username());
            set.setPassword(cred->password());
          }
        }
      }
    }
    catch ( const MediaException &e )
    {
      disconnectFrom();
      ZYPP_RETHROW(e);
    }
  }

  void MediaNetworkCommonHandler::clearTransferSettings()
  {
    for( OriginEndpoint &u : _origin ) {
      u.eraseConfigValue ( MIRR_SETTINGS_KEY.data() );
    }
  }


  bool MediaNetworkCommonHandler::authenticate(const Url &url, TransferSettings &settings, const std::string & availAuthTypes, bool firstTry)
  {
    //! \todo need a way to pass different CredManagerOptions here
    CredentialManager cm(CredManagerOptions(ZConfig::instance().repoManagerRoot()));
    return authenticate( url, cm, settings, availAuthTypes, firstTry );
  }

  std::vector<unsigned int> MediaNetworkCommonHandler::mirrorOrder(const OnMediaLocation &loc) const
  {
    std::vector<unsigned> mirrOrder;
    uint authCount = _origin.authorityCount();

    if ( !loc.mirrorsAllowed () ) {
      MIL << "Fetching file " << loc << " from authorities only ( " << authCount << " ): " << _origin << std::endl;
      mirrOrder.reserve( authCount );
      for( unsigned i = 0; i < authCount ; i++ ) { mirrOrder.push_back(i); }
    } else {
      mirrOrder.reserve( _origin.endpointCount() );
      // mirrors first
      for( unsigned i = authCount; i < _origin.endpointCount () ; i++ ) { mirrOrder.push_back(i) ;}
      // authorities last
      for( unsigned i = 0; i < authCount ; i++ ) { mirrOrder.push_back(i); }
    }
    return mirrOrder;
  }

  bool MediaNetworkCommonHandler::authenticate( const Url &url, CredentialManager &cm, TransferSettings &settings, const std::string & availAuthTypes, bool firstTry )
  {
    CurlAuthData_Ptr credentials;

    // get stored credentials
    AuthData_Ptr cmcred = cm.getCred(url);

    if (cmcred && firstTry)
    {
      credentials.reset(new CurlAuthData(*cmcred));
      DBG << "got stored credentials:" << endl << *credentials << endl;
    }
    // if not found, ask user
    else
    {

      CurlAuthData_Ptr curlcred;
      curlcred.reset(new CurlAuthData());
      callback::SendReport<AuthenticationReport> auth_report;

      // preset the username if present in current url
      if (!url.getUsername().empty() && firstTry)
        curlcred->setUsername(url.getUsername());
      // if CM has found some credentials, preset the username from there
      else if (cmcred)
        curlcred->setUsername(cmcred->username());

      // indicate we have no good credentials from CM
      cmcred.reset();

      std::string prompt_msg = str::Format(_("Authentication required for '%s'")) % url.asString();

      // set available authentication types from the exception
      // might be needed in prompt
      curlcred->setAuthType(availAuthTypes);

      // ask user
      if (auth_report->prompt(url, prompt_msg, *curlcred))
      {
        DBG << "callback answer: retry" << endl
            << "CurlAuthData: " << *curlcred << endl;

        if (curlcred->valid())
        {
          credentials = curlcred;
          // if (credentials->username() != _url.url().getUsername())
          //   _url.url().setUsername(credentials->username());
          /**
         *  \todo find a way to save the url with changed username
         *  back to repoinfo or dont store urls with username
         *  (and either forbid more repos with the same url and different
         *  user, or return a set of credentials from CM and try them one
         *  by one)
         */
        }
      }
      else
      {
        DBG << "callback answer: cancel" << endl;
      }
    }

    // set username and password
    if (credentials)
    {
      settings.setUsername(credentials->username());
      settings.setPassword(credentials->password());

      // set available authentication types from the exception
      if (credentials->authType() == CURLAUTH_NONE)
        credentials->setAuthType(availAuthTypes);

      // set auth type (seems this must be set _after_ setting the userpwd)
      if (credentials->authType() != CURLAUTH_NONE) {
        settings.setAuthType(credentials->authTypeAsString());
      }

      if (!cmcred)
      {
        credentials->setUrl(url);
        cm.addCred(*credentials);
        cm.save();
      }

      return true;
    }

    return false;
  }


} // namespace zypp::media
