/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp/media/MediaCurl.cc
 *
*/

#include <iostream>
#include <list>

#include <zypp/base/Logger.h>
#include <zypp/ExternalProgram.h>
#include <zypp/base/String.h>
#include <zypp/base/Gettext.h>
#include <zypp/base/Sysconfig.h>
#include <zypp/base/Gettext.h>

#include <zypp/media/MediaCurl.h>
#include <zypp/media/ProxyInfo.h>
#include <zypp/media/MediaUserAuth.h>
#include <zypp/media/CredentialManager.h>
#include <zypp/media/CurlConfig.h>
#include <zypp/media/CurlHelper.h>
#include <zypp/media/mediacurlprefetcher.h>
#include <zypp/Target.h>
#include <zypp/ZYppFactory.h>
#include <zypp/ZConfig.h>

#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>

using std::endl;

using namespace internal;
using namespace zypp::base;

namespace zypp {

  namespace media {

Pathname MediaCurl::_cookieFile = "/var/lib/YaST2/cookies";

// we use this define to unbloat code as this C setting option
// and catching exception is done frequently.
/** \todo deprecate SET_OPTION and use the typed versions below. */
#define SET_OPTION(opt,val) do { \
    ret = curl_easy_setopt ( _curl, opt, val ); \
    if ( ret != 0) { \
      ZYPP_THROW(MediaCurlSetOptException(_url, _curlError)); \
    } \
  } while ( false )

#define SET_OPTION_OFFT(opt,val) SET_OPTION(opt,(curl_off_t)val)
#define SET_OPTION_LONG(opt,val) SET_OPTION(opt,(long)val)
#define SET_OPTION_VOID(opt,val) SET_OPTION(opt,(void*)val)

MediaCurl::MediaCurl( const Url &      url_r,
                      const Pathname & attach_point_hint_r )
    : MediaHandler( url_r, attach_point_hint_r,
                    "/", // urlpath at attachpoint
                    true ), // does_download
      _curl( NULL ),
      _customHeaders(0L)
{
  _curlError[0] = '\0';
  _curlDebug = 0L;

  MIL << "MediaCurl::MediaCurl(" << url_r << ", " << attach_point_hint_r << ")" << endl;

  globalInitCurlOnce();

  if( !attachPoint().empty())
  {
    PathInfo ainfo(attachPoint());
    Pathname apath(attachPoint() + "XXXXXX");
    char    *atemp = ::strdup( apath.asString().c_str());
    char    *atest = NULL;
    if( !ainfo.isDir() || !ainfo.userMayRWX() ||
         atemp == NULL || (atest=::mkdtemp(atemp)) == NULL)
    {
      WAR << "attach point " << ainfo.path()
          << " is not useable for " << url_r.getScheme() << endl;
      setAttachPoint("", true);
    }
    else if( atest != NULL)
      ::rmdir(atest);

    if( atemp != NULL)
      ::free(atemp);
  }
}

Url MediaCurl::clearQueryString(const Url &url) const
{
  return internal::clearQueryString(url);
}

TransferSettings & MediaCurl::settings()
{
    return _settings;
}


void MediaCurl::setCookieFile( const Pathname &fileName )
{
  _cookieFile = fileName;
}

///////////////////////////////////////////////////////////////////

void MediaCurl::checkProtocol(const Url &url) const
{
  curl_version_info_data *curl_info = NULL;
  curl_info = curl_version_info(CURLVERSION_NOW);
  // curl_info does not need any free (is static)
  if (curl_info->protocols)
  {
    const char * const *proto;
    std::string        scheme( url.getScheme());
    bool               found = false;
    for(proto=curl_info->protocols; !found && *proto; ++proto)
    {
      if( scheme == std::string((const char *)*proto))
        found = true;
    }
    if( !found)
    {
      std::string msg("Unsupported protocol '");
      msg += scheme;
      msg += "'";
      ZYPP_THROW(MediaBadUrlException(_url, msg));
    }
  }
}

void MediaCurl::setupEasy()
{
  {
    char *ptr = getenv("ZYPP_MEDIA_CURL_DEBUG");
    _curlDebug = (ptr && *ptr) ? str::strtonum<long>( ptr) : 0L;
    if( _curlDebug > 0)
    {
      curl_easy_setopt( _curl, CURLOPT_VERBOSE, 1L);
      curl_easy_setopt( _curl, CURLOPT_DEBUGFUNCTION, log_curl);
      curl_easy_setopt( _curl, CURLOPT_DEBUGDATA, &_curlDebug);
    }
  }

  curl_easy_setopt(_curl, CURLOPT_HEADERFUNCTION, log_redirects_curl);
  curl_easy_setopt(_curl, CURLOPT_HEADERDATA, &_lastRedirect);
  CURLcode ret = curl_easy_setopt( _curl, CURLOPT_ERRORBUFFER, _curlError );
  if ( ret != 0 ) {
    ZYPP_THROW(MediaCurlSetOptException(_url, "Error setting error buffer"));
  }

  SET_OPTION(CURLOPT_FAILONERROR, 1L);
  SET_OPTION(CURLOPT_NOSIGNAL, 1L);

  // create non persistant settings
  // so that we don't add headers twice
  TransferSettings vol_settings(_settings);

  // add custom headers for download.opensuse.org (bsc#955801)
  if ( _url.getHost() == "download.opensuse.org" )
  {
    vol_settings.addHeader(anonymousIdHeader());
    vol_settings.addHeader(distributionFlavorHeader());
  }
  vol_settings.addHeader("Pragma:");

  _settings.setTimeout(ZConfig::instance().download_transfer_timeout());
  _settings.setConnectTimeout(CONNECT_TIMEOUT);

  _settings.setUserAgentString(agentString());

  // fill some settings from url query parameters
  try
  {
      fillSettingsFromUrl(_url, _settings);
  }
  catch ( const MediaException &e )
  {
      disconnectFrom();
      ZYPP_RETHROW(e);
  }
  // if the proxy was not set (or explicitly unset) by url, then look...
  if ( _settings.proxy().empty() )
  {
      // ...at the system proxy settings
      fillSettingsSystemProxy(_url, _settings);
  }

  /** Force IPv4/v6 */
  if ( env::ZYPP_MEDIA_CURL_IPRESOLVE() )
  {
    switch ( env::ZYPP_MEDIA_CURL_IPRESOLVE() )
    {
      case 4: SET_OPTION(CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4); break;
      case 6: SET_OPTION(CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6); break;
    }
  }

 /**
  * Connect timeout
  */
  SET_OPTION(CURLOPT_CONNECTTIMEOUT, _settings.connectTimeout());
  // If a transfer timeout is set, also set CURLOPT_TIMEOUT to an upper limit
  // just in case curl does not trigger its progress callback frequently
  // enough.
  if ( _settings.timeout() )
  {
    SET_OPTION(CURLOPT_TIMEOUT, 3600L);
  }

  // follow any Location: header that the server sends as part of
  // an HTTP header (#113275)
  SET_OPTION(CURLOPT_FOLLOWLOCATION, 1L);
  // 3 redirects seem to be too few in some cases (bnc #465532)
  SET_OPTION(CURLOPT_MAXREDIRS, 6L);

  if ( _url.getScheme() == "https" )
  {
#if CURLVERSION_AT_LEAST(7,19,4)
    // restrict following of redirections from https to https only
    SET_OPTION( CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS );
#endif

    if( _settings.verifyPeerEnabled() ||
        _settings.verifyHostEnabled() )
    {
      SET_OPTION(CURLOPT_CAPATH, _settings.certificateAuthoritiesPath().c_str());
    }

    if( ! _settings.clientCertificatePath().empty() )
    {
      SET_OPTION(CURLOPT_SSLCERT, _settings.clientCertificatePath().c_str());
    }
    if( ! _settings.clientKeyPath().empty() )
    {
      SET_OPTION(CURLOPT_SSLKEY, _settings.clientKeyPath().c_str());
    }

#ifdef CURLSSLOPT_ALLOW_BEAST
    // see bnc#779177
    ret = curl_easy_setopt( _curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_ALLOW_BEAST );
    if ( ret != 0 ) {
      disconnectFrom();
      ZYPP_THROW(MediaCurlSetOptException(_url, _curlError));
    }
#endif
    SET_OPTION(CURLOPT_SSL_VERIFYPEER, _settings.verifyPeerEnabled() ? 1L : 0L);
    SET_OPTION(CURLOPT_SSL_VERIFYHOST, _settings.verifyHostEnabled() ? 2L : 0L);
    // bnc#903405 - POODLE: libzypp should only talk TLS
    SET_OPTION(CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
  }

  SET_OPTION(CURLOPT_USERAGENT, _settings.userAgentString().c_str() );

  /* Fixes bsc#1174011 "auth=basic ignored in some cases"
       * We should proactively add the password to the request if basic auth is configured
       * and a password is available in the credentials but not in the URL.
       *
       * We will be a bit paranoid here and require that the URL has a user embedded, otherwise we go the default route
       * and ask the server first about the auth method
       */
  if ( _settings.authType() == "basic"
       && _settings.username().size()
       && !_settings.password().size() ) {

    CredentialManager cm(CredManagerOptions(ZConfig::instance().repoManagerRoot()));
    const auto cred = cm.getCred( _url );
    if ( cred && cred->valid() ) {
      if ( !_settings.username().size() )
        _settings.setUsername(cred->username());
      _settings.setPassword(cred->password());
    }
  }

  /*---------------------------------------------------------------*
   CURLOPT_USERPWD: [user name]:[password]

   Url::username/password -> CURLOPT_USERPWD
   If not provided, anonymous FTP identification
   *---------------------------------------------------------------*/

  if ( _settings.userPassword().size() )
  {
    SET_OPTION(CURLOPT_USERPWD, _settings.userPassword().c_str());
    std::string use_auth = _settings.authType();
    if (use_auth.empty())
      use_auth = "digest,basic";	// our default
    long auth = CurlAuthData::auth_type_str2long(use_auth);
    if( auth != CURLAUTH_NONE)
    {
      DBG << "Enabling HTTP authentication methods: " << use_auth
	  << " (CURLOPT_HTTPAUTH=" << auth << ")" << std::endl;
      SET_OPTION(CURLOPT_HTTPAUTH, auth);
    }
  }

  if ( _settings.proxyEnabled() && ! _settings.proxy().empty() )
  {
    DBG << "Proxy: '" << _settings.proxy() << "'" << endl;
    SET_OPTION(CURLOPT_PROXY, _settings.proxy().c_str());
    SET_OPTION(CURLOPT_PROXYAUTH, CURLAUTH_BASIC|CURLAUTH_DIGEST|CURLAUTH_NTLM );
    /*---------------------------------------------------------------*
     *    CURLOPT_PROXYUSERPWD: [user name]:[password]
     *
     * Url::option(proxyuser and proxypassword) -> CURLOPT_PROXYUSERPWD
     *  If not provided, $HOME/.curlrc is evaluated
     *---------------------------------------------------------------*/

    std::string proxyuserpwd = _settings.proxyUserPassword();

    if ( proxyuserpwd.empty() )
    {
      CurlConfig curlconf;
      CurlConfig::parseConfig(curlconf); // parse ~/.curlrc
      if ( curlconf.proxyuserpwd.empty() )
	DBG << "Proxy: ~/.curlrc does not contain the proxy-user option" << endl;
      else
      {
	proxyuserpwd = curlconf.proxyuserpwd;
	DBG << "Proxy: using proxy-user from ~/.curlrc" << endl;
      }
    }
    else
    {
      DBG << "Proxy: using provided proxy-user '" << _settings.proxyUsername() << "'" << endl;
    }

    if ( ! proxyuserpwd.empty() )
    {
      SET_OPTION(CURLOPT_PROXYUSERPWD, curlUnEscape( proxyuserpwd ).c_str());
    }
  }
#if CURLVERSION_AT_LEAST(7,19,4)
  else if ( _settings.proxy() == EXPLICITLY_NO_PROXY )
  {
    // Explicitly disabled in URL (see fillSettingsFromUrl()).
    // This should also prevent libcurl from looking into the environment.
    DBG << "Proxy: explicitly NOPROXY" << endl;
    SET_OPTION(CURLOPT_NOPROXY, "*");
  }
#endif
  else
  {
    DBG << "Proxy: not explicitly set" << endl;
    DBG << "Proxy: libcurl may look into the environment" << endl;
  }

  /** Speed limits */
  if ( _settings.minDownloadSpeed() != 0 )
  {
      SET_OPTION(CURLOPT_LOW_SPEED_LIMIT, _settings.minDownloadSpeed());
      // default to 10 seconds at low speed
      SET_OPTION(CURLOPT_LOW_SPEED_TIME, 60L);
  }

#if CURLVERSION_AT_LEAST(7,15,5)
  if ( _settings.maxDownloadSpeed() != 0 )
      SET_OPTION_OFFT(CURLOPT_MAX_RECV_SPEED_LARGE, _settings.maxDownloadSpeed());
#endif

  /*---------------------------------------------------------------*
   *---------------------------------------------------------------*/

  _currentCookieFile = _cookieFile.asString();
  filesystem::assert_file_mode( _currentCookieFile, 0600 );
  if ( str::strToBool( _url.getQueryParam( "cookies" ), true ) )
    SET_OPTION(CURLOPT_COOKIEFILE, _currentCookieFile.c_str() );
  else
    MIL << "No cookies requested" << endl;
  SET_OPTION(CURLOPT_COOKIEJAR, _currentCookieFile.c_str() );
  SET_OPTION(CURLOPT_PROGRESSFUNCTION, &progressCallback );
  SET_OPTION(CURLOPT_NOPROGRESS, 0L);

#if CURLVERSION_AT_LEAST(7,18,0)
  // bnc #306272
    SET_OPTION(CURLOPT_PROXY_TRANSFER_MODE, 1L );
#endif
  // append settings custom headers to curl
  for ( TransferSettings::Headers::const_iterator it = vol_settings.headersBegin();
        it != vol_settings.headersEnd();
        ++it )
  {
    // MIL << "HEADER " << *it << std::endl;

      _customHeaders = curl_slist_append(_customHeaders, it->c_str());
      if ( !_customHeaders )
          ZYPP_THROW(MediaCurlInitException(_url));
  }

  SET_OPTION(CURLOPT_HTTPHEADER, _customHeaders);
}

///////////////////////////////////////////////////////////////////


void MediaCurl::attachTo (bool next)
{
  if ( next )
    ZYPP_THROW(MediaNotSupportedException(_url));

  if ( !_url.isValid() )
    ZYPP_THROW(MediaBadUrlException(_url));

  checkProtocol(_url);
  if( !isUseableAttachPoint( attachPoint() ) )
  {
    setAttachPoint( createAttachPoint(), true );
  }

  disconnectFrom(); // clean _curl if needed
  _curl = curl_easy_init();
  if ( !_curl ) {
    ZYPP_THROW(MediaCurlInitException(_url));
  }
  try
    {
      setupEasy();
    }
  catch (Exception & ex)
    {
      disconnectFrom();
      ZYPP_RETHROW(ex);
    }

  // FIXME: need a derived class to propelly compare url's
  MediaSourceRef media( new MediaSource(_url.getScheme(), _url.asString()));
  setMediaSource(media);

  _prefetchCacheId = MediaCurlPrefetcher::instance().createCache();
}

bool
MediaCurl::checkAttachPoint(const Pathname &apoint) const
{
  return MediaHandler::checkAttachPoint( apoint, true, true);
}

///////////////////////////////////////////////////////////////////

void MediaCurl::disconnectFrom()
{
  if ( _customHeaders )
  {
    curl_slist_free_all(_customHeaders);
    _customHeaders = 0L;
  }

  if ( _curl )
  {
    curl_easy_cleanup( _curl );
    _curl = NULL;
  }

  if ( _prefetchCacheId ) {
    MediaCurlPrefetcher::instance().closeCache( _prefetchCacheId.get() );
    _prefetchCacheId.reset();
  }
}

///////////////////////////////////////////////////////////////////

void MediaCurl::releaseFrom( const std::string & ejectDev )
{
  disconnect();
}

Url MediaCurl::getFileUrl( const Pathname & filename_r ) const
{
  // Simply extend the URLs pathname. An 'absolute' URL path
  // is achieved by encoding the leading '/' in an URL path:
  //   URL: ftp://user@server		-> ~user
  //   URL: ftp://user@server/		-> ~user
  //   URL: ftp://user@server//		-> ~user
  //   URL: ftp://user@server/%2F	-> /
  //                         ^- this '/' is just a separator
  Url newurl( _url );
  newurl.setPathName( ( Pathname("./"+_url.getPathName()) / filename_r ).asString().substr(1) );
  return newurl;
}

///////////////////////////////////////////////////////////////////

void MediaCurl::getFile(const Pathname & filename , const ByteCount &expectedFileSize_r) const
{
    // Use absolute file name to prevent access of files outside of the
    // hierarchy below the attach point.
    getFileCopy(filename, localPath(filename).absolutename(), expectedFileSize_r);
}

///////////////////////////////////////////////////////////////////

void MediaCurl::getFileCopy( const Pathname & filename , const Pathname & target, const ByteCount &expectedFileSize_r ) const
{
  callback::SendReport<DownloadProgressReport> report;

  Url fileurl(getFileUrl(filename));

  if ( _prefetchCacheId && MediaCurlPrefetcher::instance().requireFile( _prefetchCacheId.get(), fileurl, target, report ) ) {
    MIL << "Got file " << filename << " from precache." << std::endl;
    return;
  } else {
    MIL << "Precache failed for file " << filename << std::endl;
  }

  bool retry = false;

  do
  {
    try
    {
      doGetFileCopy(filename, target, report, expectedFileSize_r);
      retry = false;
    }
    // retry with proper authentication data
    catch (MediaUnauthorizedException & ex_r)
    {
      if(authenticate(ex_r.hint(), !retry))
        retry = true;
      else
      {
        report->finish(fileurl, zypp::media::DownloadProgressReport::ACCESS_DENIED, ex_r.asUserHistory());
        ZYPP_RETHROW(ex_r);
      }
    }
    // unexpected exception
    catch (MediaException & excpt_r)
    {
      media::DownloadProgressReport::Error reason = media::DownloadProgressReport::ERROR;
      if( typeid(excpt_r) == typeid( media::MediaFileNotFoundException )  ||
	  typeid(excpt_r) == typeid( media::MediaNotAFileException ) )
      {
	reason = media::DownloadProgressReport::NOT_FOUND;
      }
      report->finish(fileurl, reason, excpt_r.asUserHistory());
      ZYPP_RETHROW(excpt_r);
    }
  }
  while (retry);

  report->finish(fileurl, zypp::media::DownloadProgressReport::NO_ERROR, "");
}

///////////////////////////////////////////////////////////////////

bool MediaCurl::getDoesFileExist( const Pathname & filename ) const
{
  bool retry = false;

  do
  {
    try
    {
      return doGetDoesFileExist( filename );
    }
    // authentication problem, retry with proper authentication data
    catch (MediaUnauthorizedException & ex_r)
    {
      if(authenticate(ex_r.hint(), !retry))
        retry = true;
      else
        ZYPP_RETHROW(ex_r);
    }
    // unexpected exception
    catch (MediaException & excpt_r)
    {
      ZYPP_RETHROW(excpt_r);
    }
  }
  while (retry);

  return false;
}

///////////////////////////////////////////////////////////////////

void MediaCurl::evaluateCurlCode(const Pathname &filename,
                                  CURLcode code,
                                  bool timeout_reached) const
{
  if ( code != 0 )
  {
    Url url;
    if (filename.empty())
      url = _url;
    else
      url = getFileUrl(filename);

    std::string err;
    {
      switch ( code )
      {
      case CURLE_UNSUPPORTED_PROTOCOL:
	  err = " Unsupported protocol";
	  if ( !_lastRedirect.empty() )
	  {
	    err += " or redirect (";
	    err += _lastRedirect;
	    err += ")";
	  }
	  break;
      case CURLE_URL_MALFORMAT:
      case CURLE_URL_MALFORMAT_USER:
          err = " Bad URL";
          break;
      case CURLE_LOGIN_DENIED:
          ZYPP_THROW(
              MediaUnauthorizedException(url, "Login failed.", _curlError, ""));
          break;
      case CURLE_HTTP_RETURNED_ERROR:
      {
        long httpReturnCode = 0;
        CURLcode infoRet = curl_easy_getinfo( _curl,
                                              CURLINFO_RESPONSE_CODE,
                                              &httpReturnCode );
        if ( infoRet == CURLE_OK )
        {
          std::string msg = "HTTP response: " + str::numstring( httpReturnCode );
          switch ( httpReturnCode )
          {
          case 401:
          {
            std::string auth_hint = getAuthHint();

            DBG << msg << " Login failed (URL: " << url.asString() << ")" << std::endl;
            DBG << "MediaUnauthorizedException auth hint: '" << auth_hint << "'" << std::endl;

            ZYPP_THROW(MediaUnauthorizedException(
                           url, "Login failed.", _curlError, auth_hint
                           ));
          }

          case 502: // bad gateway (bnc #1070851)
          case 503: // service temporarily unavailable (bnc #462545)
            ZYPP_THROW(MediaTemporaryProblemException(url));
          case 504: // gateway timeout
            ZYPP_THROW(MediaTimeoutException(url));
          case 403:
          {
            std::string msg403;
	    if ( url.getHost().find(".suse.com") != std::string::npos )
	      msg403 = _("Visit the SUSE Customer Center to check whether your registration is valid and has not expired.");
	    else if (url.asString().find("novell.com") != std::string::npos)
              msg403 = _("Visit the Novell Customer Center to check whether your registration is valid and has not expired.");
            ZYPP_THROW(MediaForbiddenException(url, msg403));
          }
          case 404:
          case 410:
              ZYPP_THROW(MediaFileNotFoundException(_url, filename));
          }

          DBG << msg << " (URL: " << url.asString() << ")" << std::endl;
          ZYPP_THROW(MediaCurlException(url, msg, _curlError));
        }
        else
        {
          std::string msg = "Unable to retrieve HTTP response:";
          DBG << msg << " (URL: " << url.asString() << ")" << std::endl;
          ZYPP_THROW(MediaCurlException(url, msg, _curlError));
        }
      }
      break;
      case CURLE_FTP_COULDNT_RETR_FILE:
#if CURLVERSION_AT_LEAST(7,16,0)
      case CURLE_REMOTE_FILE_NOT_FOUND:
#endif
      case CURLE_FTP_ACCESS_DENIED:
      case CURLE_TFTP_NOTFOUND:
        err = "File not found";
        ZYPP_THROW(MediaFileNotFoundException(_url, filename));
        break;
      case CURLE_BAD_PASSWORD_ENTERED:
      case CURLE_FTP_USER_PASSWORD_INCORRECT:
          err = "Login failed";
          break;
      case CURLE_COULDNT_RESOLVE_PROXY:
      case CURLE_COULDNT_RESOLVE_HOST:
      case CURLE_COULDNT_CONNECT:
      case CURLE_FTP_CANT_GET_HOST:
        err = "Connection failed";
        break;
      case CURLE_WRITE_ERROR:
        err = "Write error";
        break;
      case CURLE_PARTIAL_FILE:
      case CURLE_OPERATION_TIMEDOUT:
	timeout_reached	= true; // fall though to TimeoutException
	// fall though...
      case CURLE_ABORTED_BY_CALLBACK:
         if( timeout_reached )
        {
          err  = "Timeout reached";
          ZYPP_THROW(MediaTimeoutException(url));
        }
        else
        {
          err = "User abort";
        }
        break;
      case CURLE_SSL_PEER_CERTIFICATE:
      default:
        err = "Curl error " + str::numstring( code );
        break;
      }

      // uhm, no 0 code but unknown curl exception
      ZYPP_THROW(MediaCurlException(url, err, _curlError));
    }
  }
  else
  {
    // actually the code is 0, nothing happened
  }
}

///////////////////////////////////////////////////////////////////

bool MediaCurl::doGetDoesFileExist( const Pathname & filename ) const
{
  DBG << filename.asString() << endl;

  if(!_url.isValid())
    ZYPP_THROW(MediaBadUrlException(_url));

  if(_url.getHost().empty())
    ZYPP_THROW(MediaBadUrlEmptyHostException(_url));

  Url url(getFileUrl(filename));

  DBG << "URL: " << url.asString() << endl;
    // Use URL without options and without username and passwd
    // (some proxies dislike them in the URL).
    // Curl seems to need the just scheme, hostname and a path;
    // the rest was already passed as curl options (in attachTo).
  Url curlUrl( clearQueryString(url) );

  //
    // See also Bug #154197 and ftp url definition in RFC 1738:
    // The url "ftp://user@host/foo/bar/file" contains a path,
    // that is relative to the user's home.
    // The url "ftp://user@host//foo/bar/file" (or also with
    // encoded slash as %2f) "ftp://user@host/%2ffoo/bar/file"
    // contains an absolute path.
  //
  _lastRedirect.clear();
  std::string urlBuffer( curlUrl.asString());
  CURLcode ret = curl_easy_setopt( _curl, CURLOPT_URL,
                                   urlBuffer.c_str() );
  if ( ret != 0 ) {
    ZYPP_THROW(MediaCurlSetOptException(url, _curlError));
  }

  // instead of returning no data with NOBODY, we return
  // little data, that works with broken servers, and
  // works for ftp as well, because retrieving only headers
  // ftp will return always OK code ?
  // See http://curl.haxx.se/docs/knownbugs.html #58
  if (  (_url.getScheme() == "http" ||  _url.getScheme() == "https") &&
        _settings.headRequestsAllowed() )
    ret = curl_easy_setopt( _curl, CURLOPT_NOBODY, 1L );
  else
    ret = curl_easy_setopt( _curl, CURLOPT_RANGE, "0-1" );

  if ( ret != 0 ) {
    curl_easy_setopt( _curl, CURLOPT_NOBODY, 0L);
    curl_easy_setopt( _curl, CURLOPT_RANGE, NULL );
    /* yes, this is why we never got to get NOBODY working before,
       because setting it changes this option too, and we also
       need to reset it
       See: http://curl.haxx.se/mail/archive-2005-07/0073.html
    */
    curl_easy_setopt( _curl, CURLOPT_HTTPGET, 1L );
    ZYPP_THROW(MediaCurlSetOptException(url, _curlError));
  }

  AutoFILE file { ::fopen( "/dev/null", "w" ) };
  if ( !file ) {
      ERR << "fopen failed for /dev/null" << endl;
      curl_easy_setopt( _curl, CURLOPT_NOBODY, 0L);
      curl_easy_setopt( _curl, CURLOPT_RANGE, NULL );
      /* yes, this is why we never got to get NOBODY working before,
       because setting it changes this option too, and we also
       need to reset it
       See: http://curl.haxx.se/mail/archive-2005-07/0073.html
      */
      curl_easy_setopt( _curl, CURLOPT_HTTPGET, 1L );
      if ( ret != 0 ) {
          ZYPP_THROW(MediaCurlSetOptException(url, _curlError));
      }
      ZYPP_THROW(MediaWriteException("/dev/null"));
  }

  ret = curl_easy_setopt( _curl, CURLOPT_WRITEDATA, (*file) );
  if ( ret != 0 ) {
      std::string err( _curlError);
      curl_easy_setopt( _curl, CURLOPT_RANGE, NULL );
      curl_easy_setopt( _curl, CURLOPT_NOBODY, 0L);
      /* yes, this is why we never got to get NOBODY working before,
       because setting it changes this option too, and we also
       need to reset it
       See: http://curl.haxx.se/mail/archive-2005-07/0073.html
      */
      curl_easy_setopt( _curl, CURLOPT_HTTPGET, 1L );
      if ( ret != 0 ) {
          ZYPP_THROW(MediaCurlSetOptException(url, _curlError));
      }
      ZYPP_THROW(MediaCurlSetOptException(url, err));
  }

  CURLcode ok = curl_easy_perform( _curl );
  MIL << "perform code: " << ok << " [ " << curl_easy_strerror(ok) << " ]" << endl;

  // reset curl settings
  if (  _url.getScheme() == "http" ||  _url.getScheme() == "https" )
  {
    curl_easy_setopt( _curl, CURLOPT_NOBODY, 0L);
    if ( ret != 0 ) {
      ZYPP_THROW(MediaCurlSetOptException(url, _curlError));
    }

    /* yes, this is why we never got to get NOBODY working before,
       because setting it changes this option too, and we also
       need to reset it
       See: http://curl.haxx.se/mail/archive-2005-07/0073.html
    */
    curl_easy_setopt( _curl, CURLOPT_HTTPGET, 1L);
    if ( ret != 0 ) {
      ZYPP_THROW(MediaCurlSetOptException(url, _curlError));
    }

  }
  else
  {
    // for FTP we set different options
    curl_easy_setopt( _curl, CURLOPT_RANGE, NULL);
    if ( ret != 0 ) {
      ZYPP_THROW(MediaCurlSetOptException(url, _curlError));
    }
  }

  // as we are not having user interaction, the user can't cancel
  // the file existence checking, a callback or timeout return code
  // will be always a timeout.
  try {
      evaluateCurlCode( filename, ok, true /* timeout */);
  }
  catch ( const MediaFileNotFoundException &e ) {
      // if the file did not exist then we can return false
      return false;
  }
  catch ( const MediaException &e ) {
      // some error, we are not sure about file existence, rethrw
      ZYPP_RETHROW(e);
  }
  // exists
  return ( ok == CURLE_OK );
}

///////////////////////////////////////////////////////////////////


#if DETECT_DIR_INDEX
bool MediaCurl::detectDirIndex() const
{
  if(_url.getScheme() != "http" && _url.getScheme() != "https")
    return false;
  //
  // try to check the effective url and set the not_a_file flag
  // if the url path ends with a "/", what usually means, that
  // we've received a directory index (index.html content).
  //
  // Note: This may be dangerous and break file retrieving in
  //       case of some server redirections ... ?
  //
  bool      not_a_file = false;
  char     *ptr = NULL;
  CURLcode  ret = curl_easy_getinfo( _curl,
				     CURLINFO_EFFECTIVE_URL,
				     &ptr);
  if ( ret == CURLE_OK && ptr != NULL)
  {
    try
    {
      Url         eurl( ptr);
      std::string path( eurl.getPathName());
      if( !path.empty() && path != "/" && *path.rbegin() == '/')
      {
	DBG << "Effective url ("
	    << eurl
	    << ") seems to provide the index of a directory"
	    << endl;
	not_a_file = true;
      }
    }
    catch( ... )
    {}
  }
  return not_a_file;
}
#endif

///////////////////////////////////////////////////////////////////

void MediaCurl::doGetFileCopy(const Pathname & filename , const Pathname & target, callback::SendReport<DownloadProgressReport> & report, const ByteCount &expectedFileSize_r, RequestOptions options ) const
{
    Pathname dest = target.absolutename();
    if( assert_dir( dest.dirname() ) )
    {
      DBG << "assert_dir " << dest.dirname() << " failed" << endl;
      ZYPP_THROW( MediaSystemException(getFileUrl(filename), "System error on " + dest.dirname().asString()) );
    }

    ManagedFile destNew { target.extend( ".new.zypp.XXXXXX" ) };
    AutoFILE file;
    {
      AutoFREE<char> buf { ::strdup( (*destNew).c_str() ) };
      if( ! buf )
      {
	ERR << "out of memory for temp file name" << endl;
	ZYPP_THROW(MediaSystemException(getFileUrl(filename), "out of memory for temp file name"));
      }

      AutoFD tmp_fd { ::mkostemp( buf, O_CLOEXEC ) };
      if( tmp_fd == -1 )
      {
	ERR << "mkstemp failed for file '" << destNew << "'" << endl;
	ZYPP_THROW(MediaWriteException(destNew));
      }
      destNew = ManagedFile( (*buf), filesystem::unlink );

      file = ::fdopen( tmp_fd, "we" );
      if ( ! file )
      {
	ERR << "fopen failed for file '" << destNew << "'" << endl;
	ZYPP_THROW(MediaWriteException(destNew));
      }
      tmp_fd.resetDispose();	// don't close it here! ::fdopen moved ownership to file
    }

    DBG << "dest: " << dest << endl;
    DBG << "temp: " << destNew << endl;

    // set IFMODSINCE time condition (no download if not modified)
    if( PathInfo(target).isExist() && !(options & OPTION_NO_IFMODSINCE) )
    {
      curl_easy_setopt(_curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
      curl_easy_setopt(_curl, CURLOPT_TIMEVALUE, (long)PathInfo(target).mtime());
    }
    else
    {
      curl_easy_setopt(_curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
      curl_easy_setopt(_curl, CURLOPT_TIMEVALUE, 0L);
    }
    try
    {
      doGetFileCopyFile(filename, dest, file, report, expectedFileSize_r, options);
    }
    catch (Exception &e)
    {
      curl_easy_setopt(_curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
      curl_easy_setopt(_curl, CURLOPT_TIMEVALUE, 0L);
      ZYPP_RETHROW(e);
    }

    long httpReturnCode = 0;
    CURLcode infoRet = curl_easy_getinfo(_curl,
                                         CURLINFO_RESPONSE_CODE,
                                         &httpReturnCode);
    bool modified = true;
    if (infoRet == CURLE_OK)
    {
      DBG << "HTTP response: " + str::numstring(httpReturnCode);
      if ( httpReturnCode == 304
           || ( httpReturnCode == 213 && (_url.getScheme() == "ftp" || _url.getScheme() == "tftp") ) ) // not modified
      {
        DBG << " Not modified.";
        modified = false;
      }
      DBG << endl;
    }
    else
    {
      WAR << "Could not get the reponse code." << endl;
    }

    if (modified || infoRet != CURLE_OK)
    {
      // apply umask
      if ( ::fchmod( ::fileno(file), filesystem::applyUmaskTo( 0644 ) ) )
      {
        ERR << "Failed to chmod file " << destNew << endl;
      }

      file.resetDispose();	// we're going to close it manually here
      if ( ::fclose( file ) )
      {
        ERR << "Fclose failed for file '" << destNew << "'" << endl;
        ZYPP_THROW(MediaWriteException(destNew));
      }

      // move the temp file into dest
      if ( rename( destNew, dest ) != 0 ) {
        ERR << "Rename failed" << endl;
        ZYPP_THROW(MediaWriteException(dest));
      }
      destNew.resetDispose();	// no more need to unlink it
    }

    DBG << "done: " << PathInfo(dest) << endl;
}

///////////////////////////////////////////////////////////////////

void MediaCurl::doGetFileCopyFile(const Pathname & filename , const Pathname & dest, FILE *file, callback::SendReport<DownloadProgressReport> & report, const ByteCount &expectedFileSize_r, RequestOptions options ) const
{
    DBG << filename.asString() << endl;

    if(!_url.isValid())
      ZYPP_THROW(MediaBadUrlException(_url));

    if(_url.getHost().empty())
      ZYPP_THROW(MediaBadUrlEmptyHostException(_url));

    Url url(getFileUrl(filename));

    DBG << "URL: " << url.asString() << endl;
    // Use URL without options and without username and passwd
    // (some proxies dislike them in the URL).
    // Curl seems to need the just scheme, hostname and a path;
    // the rest was already passed as curl options (in attachTo).
    Url curlUrl( clearQueryString(url) );

    //
    // See also Bug #154197 and ftp url definition in RFC 1738:
    // The url "ftp://user@host/foo/bar/file" contains a path,
    // that is relative to the user's home.
    // The url "ftp://user@host//foo/bar/file" (or also with
    // encoded slash as %2f) "ftp://user@host/%2ffoo/bar/file"
    // contains an absolute path.
    //
    _lastRedirect.clear();
    std::string urlBuffer( curlUrl.asString());
    CURLcode ret = curl_easy_setopt( _curl, CURLOPT_URL,
                                     urlBuffer.c_str() );
    if ( ret != 0 ) {
      ZYPP_THROW(MediaCurlSetOptException(url, _curlError));
    }

    ret = curl_easy_setopt( _curl, CURLOPT_WRITEDATA, file );
    if ( ret != 0 ) {
      ZYPP_THROW(MediaCurlSetOptException(url, _curlError));
    }

    // Set callback and perform.
    internal::ProgressData progressData(_curl, _settings.timeout(), url, expectedFileSize_r, &report);
    if (!(options & OPTION_NO_REPORT_START))
      report->start(url, dest);
    if ( curl_easy_setopt( _curl, CURLOPT_PROGRESSDATA, &progressData ) != 0 ) {
      WAR << "Can't set CURLOPT_PROGRESSDATA: " << _curlError << endl;;
    }

    ret = curl_easy_perform( _curl );
#if CURLVERSION_AT_LEAST(7,19,4)
    // bnc#692260: If the client sends a request with an If-Modified-Since header
    // with a future date for the server, the server may respond 200 sending a
    // zero size file.
    // curl-7.19.4 introduces CURLINFO_CONDITION_UNMET to check this condition.
    if ( ftell(file) == 0 && ret == 0 )
    {
      long httpReturnCode = 33;
      if ( curl_easy_getinfo( _curl, CURLINFO_RESPONSE_CODE, &httpReturnCode ) == CURLE_OK && httpReturnCode == 200 )
      {
	long conditionUnmet = 33;
	if ( curl_easy_getinfo( _curl, CURLINFO_CONDITION_UNMET, &conditionUnmet ) == CURLE_OK && conditionUnmet )
	{
	  WAR << "TIMECONDITION unmet - retry without." << endl;
	  curl_easy_setopt(_curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
	  curl_easy_setopt(_curl, CURLOPT_TIMEVALUE, 0L);
	  ret = curl_easy_perform( _curl );
	}
      }
    }
#endif

    if ( curl_easy_setopt( _curl, CURLOPT_PROGRESSDATA, NULL ) != 0 ) {
      WAR << "Can't unset CURLOPT_PROGRESSDATA: " << _curlError << endl;;
    }

    if ( ret != 0 )
    {
      ERR << "curl error: " << ret << ": " << _curlError
          << ", temp file size " << ftell(file)
          << " bytes." << endl;

      // the timeout is determined by the progress data object
      // which holds whether the timeout was reached or not,
      // otherwise it would be a user cancel
      try {

        if ( progressData.fileSizeExceeded )
          ZYPP_THROW(MediaFileSizeExceededException(url, progressData._expectedFileSize));

        evaluateCurlCode( filename, ret, progressData.reached );
      }
      catch ( const MediaException &e ) {
        // some error, we are not sure about file existence, rethrw
        ZYPP_RETHROW(e);
      }
    }

#if DETECT_DIR_INDEX
    if (!ret && detectDirIndex())
      {
	ZYPP_THROW(MediaNotAFileException(_url, filename));
      }
#endif // DETECT_DIR_INDEX
}

///////////////////////////////////////////////////////////////////

void MediaCurl::getDir( const Pathname & dirname, bool recurse_r ) const
{
  filesystem::DirContent content;
  getDirInfo( content, dirname, /*dots*/false );

  for ( filesystem::DirContent::const_iterator it = content.begin(); it != content.end(); ++it ) {
      Pathname filename = dirname + it->name;
      int res = 0;

      switch ( it->type ) {
      case filesystem::FT_NOT_AVAIL: // old directory.yast contains no typeinfo at all
      case filesystem::FT_FILE:
        getFile( filename, 0 );
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

///////////////////////////////////////////////////////////////////

void MediaCurl::getDirInfo( std::list<std::string> & retlist,
                               const Pathname & dirname, bool dots ) const
{
  getDirectoryYast( retlist, dirname, dots );
}

///////////////////////////////////////////////////////////////////

void MediaCurl::getDirInfo( filesystem::DirContent & retlist,
                            const Pathname & dirname, bool dots ) const
{
  getDirectoryYast( retlist, dirname, dots );
}

///////////////////////////////////////////////////////////////////
//
int MediaCurl::aliveCallback( void *clientp, double /*dltotal*/, double dlnow, double /*ultotal*/, double /*ulnow*/ )
{
  internal::ProgressData *pdata = reinterpret_cast<internal::ProgressData *>( clientp );
  if( pdata )
  {
    // Do not propagate dltotal in alive callbacks. MultiCurl uses this to
    // prevent a percentage raise while downloading a metalink file. Download
    // activity however is indicated by propagating the download rate (via dlnow).
    pdata->updateStats( 0.0, dlnow );
    return pdata->reportProgress();
  }
  return 0;
}

int MediaCurl::progressCallback( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow )
{
  internal::ProgressData *pdata = reinterpret_cast<internal::ProgressData *>( clientp );
  if( pdata )
  {
    // work around curl bug that gives us old data
    long httpReturnCode = 0;
    if ( curl_easy_getinfo( pdata->curl, CURLINFO_RESPONSE_CODE, &httpReturnCode ) != CURLE_OK || httpReturnCode == 0 )
      return aliveCallback( clientp, dltotal, dlnow, ultotal, ulnow );

    pdata->updateStats( dltotal, dlnow );
    return pdata->reportProgress();
  }
  return 0;
}

CURL *MediaCurl::progressCallback_getcurl( void *clientp )
{
  internal::ProgressData *pdata = reinterpret_cast<internal::ProgressData *>(clientp);
  return pdata ? pdata->curl : 0;
}

///////////////////////////////////////////////////////////////////

std::string MediaCurl::getAuthHint() const
{
  long auth_info = CURLAUTH_NONE;

  CURLcode infoRet =
    curl_easy_getinfo(_curl, CURLINFO_HTTPAUTH_AVAIL, &auth_info);

  if(infoRet == CURLE_OK)
  {
    return CurlAuthData::auth_type_long2str(auth_info);
  }

  return "";
}

/**
 * MediaMultiCurl needs to reset the expected filesize in case a metalink file is downloaded
 * otherwise this function should not be called
 */
void MediaCurl::resetExpectedFileSize(void *clientp, const ByteCount &expectedFileSize)
{
  internal::ProgressData *data = reinterpret_cast<internal::ProgressData *>(clientp);
  if ( data ) {
    data->_expectedFileSize = expectedFileSize;
  }
}

///////////////////////////////////////////////////////////////////

bool MediaCurl::authenticate(const std::string & availAuthTypes, bool firstTry) const
{
  //! \todo need a way to pass different CredManagerOptions here
  CredentialManager cm(CredManagerOptions(ZConfig::instance().repoManagerRoot()));
  CurlAuthData_Ptr credentials;

  // get stored credentials
  AuthData_Ptr cmcred = cm.getCred(_url);

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
    if (!_url.getUsername().empty() && firstTry)
      curlcred->setUsername(_url.getUsername());
    // if CM has found some credentials, preset the username from there
    else if (cmcred)
      curlcred->setUsername(cmcred->username());

    // indicate we have no good credentials from CM
    cmcred.reset();

    std::string prompt_msg = str::Format(_("Authentication required for '%s'")) % _url.asString();

    // set available authentication types from the exception
    // might be needed in prompt
    curlcred->setAuthType(availAuthTypes);

    // ask user
    if (auth_report->prompt(_url, prompt_msg, *curlcred))
    {
      DBG << "callback answer: retry" << endl
          << "CurlAuthData: " << *curlcred << endl;

      if (curlcred->valid())
      {
        credentials = curlcred;
          // if (credentials->username() != _url.getUsername())
          //   _url.setUsername(credentials->username());
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
    // HACK, why is this const?
    const_cast<MediaCurl*>(this)->_settings.setUsername(credentials->username());
    const_cast<MediaCurl*>(this)->_settings.setPassword(credentials->password());

    // set username and password
    CURLcode ret = curl_easy_setopt(_curl, CURLOPT_USERPWD, _settings.userPassword().c_str());
    if ( ret != 0 ) ZYPP_THROW(MediaCurlSetOptException(_url, _curlError));

    // set available authentication types from the exception
    if (credentials->authType() == CURLAUTH_NONE)
      credentials->setAuthType(availAuthTypes);

    // set auth type (seems this must be set _after_ setting the userpwd)
    if (credentials->authType() != CURLAUTH_NONE)
    {
      // FIXME: only overwrite if not empty?
      const_cast<MediaCurl*>(this)->_settings.setAuthType(credentials->authTypeAsString());
      ret = curl_easy_setopt(_curl, CURLOPT_HTTPAUTH, credentials->authType());
      if ( ret != 0 ) ZYPP_THROW(MediaCurlSetOptException(_url, _curlError));
    }

    if (!cmcred)
    {
      credentials->setUrl(_url);
      cm.addCred(*credentials);
      cm.save();
    }

    return true;
  }

  return false;
}

//need a out of line definiton, otherwise vtable is emitted for every translation unit
MediaCurl::Callbacks::~Callbacks() {}

void zypp::media::MediaCurl::precacheFiles( const std::vector<OnMediaLocation> &files )
{
  if ( !_prefetchCacheId )
    _prefetchCacheId = MediaCurlPrefetcher::instance().createCache();
  std::vector< MediaCurlPrefetcher::Request > prefetch;
  std::for_each( files.begin(), files.end(), [this,  &prefetch ]( const OnMediaLocation &elem ){
    MediaCurlPrefetcher::Request r;
    r.cache = _prefetchCacheId.get();
    r.url = getFileUrl( elem.filename() );
    r.settings = settings();
    r.expectedFileSize = elem.downloadSize();
    prefetch.push_back( std::move(r) );
  });
  MediaCurlPrefetcher::instance().precacheFiles( std::move(prefetch) );
}


  } // namespace media
} // namespace zypp
//
