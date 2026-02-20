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
#include <chrono>
#include <list>

#include <zypp-core/base/Logger.h>
#include <zypp-core/ExternalProgram.h>
#include <zypp-core/base/String.h>
#include <zypp-core/base/Gettext.h>
#include <utility>
#include <zypp-core/parser/Sysconfig>
#include <zypp-core/base/Gettext.h>

#include <zypp/media/MediaCurl.h>
#include <zypp-core/ng/base/private/linuxhelpers_p.h>
#include <zypp-core/base/userrequestexception.h>
#include <zypp-curl/ProxyInfo>
#include <zypp-curl/auth/CurlAuthData>
#include <zypp-media/auth/CredentialManager>
#include <zypp-curl/CurlConfig>
#include <zypp/Target.h>
#include <zypp/ZYppFactory.h>
#include <zypp/ZConfig.h>
#include <zypp/zypp_detail/ZYppImpl.h> // for zypp_poll

#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <dirent.h>
#include <unistd.h>
#include <glib.h>

#include "detail/OptionalDownloadProgressReport.h"

using std::endl;

namespace internal {
  using namespace zypp;
  struct ProgressData
  {
    ProgressData( AutoFILE file, CURL *curl, time_t timeout = 0, zypp::Url  url = zypp::Url(),
                  zypp::ByteCount expectedFileSize_r = 0,
                  zypp::callback::SendReport<zypp::media::DownloadProgressReport> *_report = nullptr );

    void updateStats( curl_off_t dltotal = 0.0, curl_off_t dlnow = 0.0 );

    int reportProgress() const;

    CURL * curl()
    { return _curl; }

    bool timeoutReached() const
    { return _timeoutReached; }

    bool fileSizeExceeded() const
    { return _fileSizeExceeded; }

    ByteCount expectedFileSize() const
    { return _expectedFileSize; }

    void expectedFileSize( ByteCount newval_r )
    { _expectedFileSize = newval_r; }

    zypp::Url url() const
    { return _url; }

    FILE* file()
    { return _file.value(); }

    size_t writeBytes( char *ptr, ByteCount bytes );

    ByteCount bytesWritten() const {
      return _bytesWritten;
    }

  private:
    CURL *      _curl;
    AutoFILE    _file;
    zypp::Url	_url;
    time_t	_timeout;
    bool	_timeoutReached;
    bool        _fileSizeExceeded;
    ByteCount   _expectedFileSize;
    zypp::callback::SendReport<zypp::media::DownloadProgressReport> *report;

    time_t _timeStart	= 0;	///< Start total stats
    time_t _timeLast	= 0;	///< Start last period(~1sec)
    time_t _timeRcv	= 0;	///< Start of no-data timeout
    time_t _timeNow	= 0;	///< Now

    curl_off_t _dnlTotal = 0.0;	///< Bytes to download or 0 if unknown
    curl_off_t _dnlLast	 = 0.0;	///< Bytes downloaded at period start
    curl_off_t _dnlNow	 = 0.0;	///< Bytes downloaded now

    ByteCount _bytesWritten = 0; ///< Bytes actually written into the file

    int    _dnlPercent= 0;	///< Percent completed or 0 if _dnlTotal is unknown

    double _drateTotal= 0.0;	///< Download rate so far
    double _drateLast	= 0.0;	///< Download rate in last period
  };



  ProgressData::ProgressData(AutoFILE file, CURL *curl, time_t timeout, Url url, ByteCount expectedFileSize_r, zypp::callback::SendReport< zypp::media::DownloadProgressReport> *_report)
    : _curl( curl )
    , _file( std::move(file) )
    , _url(std::move( url ))
    , _timeout( timeout )
    , _timeoutReached( false )
    , _fileSizeExceeded ( false )
    , _expectedFileSize( expectedFileSize_r )
    , report( _report )
  {}

  void ProgressData::updateStats( curl_off_t dltotal, curl_off_t dlnow )
  {
    time_t now = _timeNow = time(0);

    // If called without args (0.0), recompute based on the last values seen
    if ( dltotal && dltotal != _dnlTotal )
      _dnlTotal = dltotal;

    if ( dlnow && dlnow != _dnlNow )
    {
      _timeRcv = now;
      _dnlNow = dlnow;
    }

    // init or reset if time jumps back
    if ( !_timeStart || _timeStart > now )
      _timeStart = _timeLast = _timeRcv = now;

    // timeout condition
    if ( _timeout )
      _timeoutReached = ( (now - _timeRcv) > _timeout );

    // percentage:
    if ( _dnlTotal )
      _dnlPercent = int( _dnlNow * 100 / _dnlTotal );

    // download rates:
    _drateTotal = double(_dnlNow) / std::max( int(now - _timeStart), 1 );

    if ( _timeLast < now )
    {
      _drateLast = double(_dnlNow - _dnlLast) / int(now - _timeLast);
      // start new period
      _timeLast  = now;
      _dnlLast   = _dnlNow;
    }
    else if ( _timeStart == _timeLast )
      _drateLast = _drateTotal;
  }

  int ProgressData::reportProgress() const
  {
    if ( _fileSizeExceeded )
      return 1;
    if ( _timeoutReached )
      return 1;	// no-data timeout
    if ( report && !(*report)->progress( _dnlPercent, _url, _drateTotal, _drateLast ) )
      return 1;	// user requested abort
    return 0;
  }

  size_t ProgressData::writeBytes(char *ptr, ByteCount bytes)
  {
    // check if the downloaded data is already bigger than what we expected
    _fileSizeExceeded = _expectedFileSize > 0 && _expectedFileSize < ( _bytesWritten + bytes );
    if ( _fileSizeExceeded )
      return 0;

    auto written = fwrite( ptr, 1, bytes, _file );
    _bytesWritten += written;
    return written;
  }

  /// Attempt to work around certain issues by autoretry in MediaCurl::getFileCopy
  /// E.g. curl error: 92: HTTP/2 PROTOCOL_ERROR as in bsc#1205843, zypper/issues/457,...
  /// ma: These errors were caused by a space terminated user agent string (bsc#1212187)
  class MediaCurlExceptionMayRetryInternaly : public media::MediaCurlException
  {
  public:
    MediaCurlExceptionMayRetryInternaly( const Url & url_r,
                                         const std::string & err_r,
                                         const std::string & msg_r )
    : media::MediaCurlException( url_r, err_r, msg_r )
    {}
    //~MediaCurlExceptionMayRetryInternaly() noexcept {}
  };

}


using namespace internal;
using namespace zypp::base;

namespace zypp {

  namespace media {

Pathname MediaCurl::_cookieFile = "/var/lib/YaST2/cookies";

// we use this define to unbloat code as this C setting option
// and catching exception is done frequently.
/** \todo deprecate SET_OPTION and use the typed versions below. */
#define SET_OPTION(opt,val) do { \
    ret = curl_easy_setopt ( curl, opt, val ); \
    if ( ret != 0) { \
      ZYPP_THROW(MediaCurlSetOptException(_origin.at(rData.mirror).url(), _curlError)); \
    } \
  } while ( false )

#define SET_OPTION_OFFT(opt,val) SET_OPTION(opt,(curl_off_t)val)
#define SET_OPTION_LONG(opt,val) SET_OPTION(opt,(long)val)
#define SET_OPTION_VOID(opt,val) SET_OPTION(opt,(void*)val)

MediaCurl::MediaCurl(const MirroredOrigin &origin_r,
                     const Pathname & attach_point_hint_r )
  : MediaNetworkCommonHandler( origin_r, attach_point_hint_r,
                                 "/", // urlpath at attachpoint
                                 true ), // does_download
      _customHeaders(0L)
{
  _multi = curl_multi_init();

  _curlError[0] = '\0';

  MIL << "MediaCurl::MediaCurl(" << origin_r.authority().url() << ", " << attach_point_hint_r << ")" << endl;

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
          << " is not useable for " << origin_r.authority().url().getScheme() << endl;
      setAttachPoint("", true);
    }
    else if( atest != NULL)
      ::rmdir(atest);

    if( atemp != NULL)
      ::free(atemp);
  }
}

MediaCurl::~MediaCurl()
{
  try { release(); } catch(...) {}
  if (_multi)
    curl_multi_cleanup(_multi);
}

void MediaCurl::setCookieFile( const Pathname &fileName )
{
  _cookieFile = fileName;
}

void MediaCurl::setCurlError(const char* error)
{
  // FIXME(dmllr): Use strlcpy if available for better performance
  strncpy(_curlError, error, sizeof(_curlError)-1);
  _curlError[sizeof(_curlError)-1] = '\0';
}

///////////////////////////////////////////////////////////////////

void MediaCurl::checkProtocol(const Url &url) const
{
  curl_version_info_data *curl_info = NULL;
  curl_info = curl_version_info(CURLVERSION_NOW);
  // curl_info does not need any free (is static)
  if (curl_info->protocols)
  {
    const char * const *proto = nullptr;
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
      ZYPP_THROW(MediaBadUrlException( url, msg));
    }
  }
}

void MediaCurl::setupEasy(RequestData &rData, TransferSettings &settings )
{
  CURL *curl = rData.curl;

  // kill old settings
  curl_easy_reset ( curl );

  ::internal::setupZYPP_MEDIA_CURL_DEBUG( curl );
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, log_redirects_curl);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &_lastRedirect);
  CURLcode ret = curl_easy_setopt( curl, CURLOPT_ERRORBUFFER, _curlError );
  if ( ret != 0 ) {
    ZYPP_THROW(MediaCurlSetOptException( _origin.at(rData.mirror).url(), "Error setting error buffer"));
  }

  SET_OPTION(CURLOPT_FAILONERROR, 1L);
  SET_OPTION(CURLOPT_NOSIGNAL, 1L);

  /** Force IPv4/v6 */
  switch ( env::ZYPP_MEDIA_CURL_IPRESOLVE() )
  {
    case 4: SET_OPTION(CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4); break;
    case 6: SET_OPTION(CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6); break;
  }

 /**
  * Connect timeout
  */
  SET_OPTION(CURLOPT_CONNECTTIMEOUT, settings.connectTimeout());
  // If a transfer timeout is set, also set CURLOPT_TIMEOUT to an upper limit
  // just in case curl does not trigger its progress callback frequently
  // enough.
  if ( settings.timeout() )
  {
    SET_OPTION(CURLOPT_TIMEOUT, 3600L);
  }

  // follow any Location: header that the server sends as part of
  // an HTTP header (#113275)
  SET_OPTION(CURLOPT_FOLLOWLOCATION, 1L);
  // 3 redirects seem to be too few in some cases (bnc #465532)
  SET_OPTION(CURLOPT_MAXREDIRS, 6L);

  if ( _origin.at(rData.mirror).url().getScheme() == "https" )
  {
    if ( :: internal::setCurlRedirProtocols ( curl ) != CURLE_OK ) {
      ZYPP_THROW(MediaCurlSetOptException(_origin.at(rData.mirror).url(), _curlError));
    }

    if( settings.verifyPeerEnabled() ||
        settings.verifyHostEnabled() )
    {
      SET_OPTION(CURLOPT_CAPATH, settings.certificateAuthoritiesPath().c_str());
    }

    if( ! settings.clientCertificatePath().empty() )
    {
      SET_OPTION(CURLOPT_SSLCERT, settings.clientCertificatePath().c_str());
    }
    if( ! settings.clientKeyPath().empty() )
    {
      SET_OPTION(CURLOPT_SSLKEY, settings.clientKeyPath().c_str());
    }

#ifdef CURLSSLOPT_ALLOW_BEAST
    // see bnc#779177
    ret = curl_easy_setopt( curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_ALLOW_BEAST );
    if ( ret != 0 ) {
      disconnectFrom();
      ZYPP_THROW(MediaCurlSetOptException(_origin.at(rData.mirror).url(), _curlError));
    }
#endif
    SET_OPTION(CURLOPT_SSL_VERIFYPEER, settings.verifyPeerEnabled() ? 1L : 0L);
    SET_OPTION(CURLOPT_SSL_VERIFYHOST, settings.verifyHostEnabled() ? 2L : 0L);
    // bnc#903405 - POODLE: libzypp should only talk TLS
    SET_OPTION(CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
  }

  SET_OPTION(CURLOPT_USERAGENT, settings.userAgentString().c_str() );

  /* Fixes bsc#1174011 "auth=basic ignored in some cases"
       * We should proactively add the password to the request if basic auth is configured
       * and a password is available in the credentials but not in the URL.
       *
       * We will be a bit paranoid here and require that the URL has a user embedded, otherwise we go the default route
       * and ask the server first about the auth method
       */
  if ( settings.authType() == "basic"
       && settings.username().size()
       && !settings.password().size() ) {

    CredentialManager cm(CredManagerOptions(ZConfig::instance().repoManagerRoot()));
    const auto cred = cm.getCred( _origin.at(rData.mirror).url() );
    if ( cred && cred->valid() ) {
      if ( !settings.username().size() )
        settings.setUsername(cred->username());
      settings.setPassword(cred->password());
    }
  }

  /*---------------------------------------------------------------*
   CURLOPT_USERPWD: [user name]:[password]

   Url::username/password -> CURLOPT_USERPWD
   If not provided, anonymous FTP identification
   *---------------------------------------------------------------*/

  if ( settings.userPassword().size() )
  {
    SET_OPTION(CURLOPT_USERPWD, settings.userPassword().c_str());
    std::string use_auth = settings.authType();
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

  if ( settings.proxyEnabled() && ! settings.proxy().empty() )
  {
    DBG << "Proxy: '" << settings.proxy() << "'" << endl;
    SET_OPTION(CURLOPT_PROXY, settings.proxy().c_str());
    SET_OPTION(CURLOPT_PROXYAUTH, CURLAUTH_BASIC|CURLAUTH_DIGEST|CURLAUTH_NTLM );
    /*---------------------------------------------------------------*
     *    CURLOPT_PROXYUSERPWD: [user name]:[password]
     *
     * Url::option(proxyuser and proxypassword) -> CURLOPT_PROXYUSERPWD
     *  If not provided, $HOME/.curlrc is evaluated
     *---------------------------------------------------------------*/

    std::string proxyuserpwd = settings.proxyUserPassword();

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
      DBG << "Proxy: using provided proxy-user '" << settings.proxyUsername() << "'" << endl;
    }

    if ( ! proxyuserpwd.empty() )
    {
      SET_OPTION(CURLOPT_PROXYUSERPWD, curlUnEscape( proxyuserpwd ).c_str());
    }
  }
#if CURLVERSION_AT_LEAST(7,19,4)
  else if ( settings.proxy() == EXPLICITLY_NO_PROXY )
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
  if ( settings.minDownloadSpeed() != 0 )
  {
      SET_OPTION(CURLOPT_LOW_SPEED_LIMIT, settings.minDownloadSpeed());
      // default to 10 seconds at low speed
      SET_OPTION(CURLOPT_LOW_SPEED_TIME, 60L);
  }

#if CURLVERSION_AT_LEAST(7,15,5)
  if ( settings.maxDownloadSpeed() != 0 )
      SET_OPTION_OFFT(CURLOPT_MAX_RECV_SPEED_LARGE, settings.maxDownloadSpeed());
#endif

  /*---------------------------------------------------------------*
   *---------------------------------------------------------------*/

  _currentCookieFile = _cookieFile.asString();
  if ( ::geteuid() == 0 || PathInfo(_currentCookieFile).owner() == ::geteuid() )
    filesystem::assert_file_mode( _currentCookieFile, 0600 );

  const auto &cookieFileParam = _origin.at(rData.mirror).url().getQueryParam( "cookies" );
  if ( !cookieFileParam.empty() && str::strToBool( cookieFileParam, true ) )
    SET_OPTION(CURLOPT_COOKIEFILE, _currentCookieFile.c_str() );
  else
    MIL << "No cookies requested" << endl;
  SET_OPTION(CURLOPT_COOKIEJAR, _currentCookieFile.c_str() );
  SET_OPTION(CURLOPT_XFERINFOFUNCTION, &progressCallback );
  SET_OPTION(CURLOPT_NOPROGRESS, 0L);

#if CURLVERSION_AT_LEAST(7,18,0)
  // bnc #306272
  SET_OPTION(CURLOPT_PROXY_TRANSFER_MODE, 1L );
#endif
  // Append settings custom headers to curl.
  // TransferSettings assert strings are trimmed (HTTP/2 RFC 9113)
  if ( _customHeaders ) {
    curl_slist_free_all(_customHeaders);
    _customHeaders = 0L;
  }
  for ( const auto &header : settings.headers() ) {
    _customHeaders = curl_slist_append(_customHeaders, header.c_str());
    if ( !_customHeaders )
      ZYPP_THROW(MediaCurlInitException(_origin.at(rData.mirror).url()));
  }
  SET_OPTION(CURLOPT_HTTPHEADER, _customHeaders);
}

///////////////////////////////////////////////////////////////////

void MediaCurl::disconnectFrom()
{
  if ( _customHeaders ) {
    curl_slist_free_all(_customHeaders);
    _customHeaders = 0L;
  }

  // clear effective settings
  clearTransferSettings();
}

///////////////////////////////////////////////////////////////////

void MediaCurl::releaseFrom( const std::string & ejectDev )
{
  disconnect();
}

///////////////////////////////////////////////////////////////////

void MediaCurl::getFileCopy( const OnMediaLocation & srcFile , const Pathname & target ) const
{
  // we need a non const pointer to work around the current API
  auto that = const_cast<MediaCurl *>(this);
  std::exception_ptr lastErr;
  const auto &mirrOrder = mirrorOrder (srcFile);
  for ( unsigned mirr : mirrOrder ) {
    try {
      return that->getFileCopyFromMirror ( mirr, srcFile, target );

    } catch (MediaException & excpt_r) {
      if ( !canTryNextMirror ( excpt_r ) )
        ZYPP_RETHROW(excpt_r);
      lastErr = ZYPP_FWD_CURRENT_EXCPT();
    }
  }
  if ( lastErr ) {
    ZYPP_RETHROW( lastErr );
  }

  // should not happen
  ZYPP_THROW( MediaException("No usable mirror available.") );

}

void MediaCurl::getFileCopyFromMirror(const int mirror, const OnMediaLocation &srcFile, const Pathname &target)
{
  const auto &filename = srcFile.filename();

  // Optional files will send no report until data are actually received (we know it exists).
  OptionalDownloadProgressReport reportfilter( srcFile.optional() );
  callback::SendReport<DownloadProgressReport> report;

  auto &endpoint = _origin[mirror];
  auto &settings = endpoint.getConfig<TransferSettings>(MIRR_SETTINGS_KEY.data());

  AutoDispose<CURL*> curl( curl_easy_init(), []( CURL *hdl ) { if ( hdl ) { curl_easy_cleanup(hdl); } }  );

  RequestData rData;
  rData.mirror = mirror;
  rData.curl = curl.value ();

  if( !endpoint.url().isValid() )
    ZYPP_THROW(MediaBadUrlException(endpoint.url()));

  if( endpoint.url().getHost().empty() )
    ZYPP_THROW(MediaBadUrlEmptyHostException(endpoint.url()));

  Url fileurl( getFileUrl(mirror, filename) );

  bool firstAuth = true;  // bsc#1210870: authenticate must not return stored credentials more than once.
  unsigned internalTry = 0;
  static constexpr unsigned maxInternalTry = 3;

  do
  {
    try
    {
      Pathname dest = target.absolutename();
      if( assert_dir( dest.dirname() ) )
      {
        DBG << "assert_dir " << dest.dirname() << " failed" << endl;
        ZYPP_THROW( MediaSystemException(fileurl, "System error on " + dest.dirname().asString()) );
      }

      ManagedFile destNew { target.extend( ".new.zypp.XXXXXX" ) };
      AutoFILE file;
      {
        AutoFREE<char> buf { ::strdup( (*destNew).c_str() ) };
        if( ! buf )
        {
          ERR << "out of memory for temp file name" << endl;
          ZYPP_THROW(MediaSystemException(fileurl, "out of memory for temp file name"));
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

      setupEasy( rData, settings );

      // set IFMODSINCE time condition (no download if not modified)
      if( PathInfo(target).isExist() )
      {
        curl_easy_setopt(curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
        curl_easy_setopt(curl, CURLOPT_TIMEVALUE, (long)PathInfo(target).mtime());
      }
      else
      {
        curl_easy_setopt(curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
        curl_easy_setopt(curl, CURLOPT_TIMEVALUE, 0L);
      }

      zypp_defer{
        curl_easy_setopt(curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
        curl_easy_setopt(curl, CURLOPT_TIMEVALUE, 0L);
      };

      DBG << srcFile.filename().asString() << endl;

      DBG << "URL: " << fileurl.asString() << endl;
      // Use URL without options and without username and passwd
      // (some proxies dislike them in the URL).
      // Curl seems to need the just scheme, hostname and a path;
      // the rest was already passed as curl options (in attachTo).
      Url curlUrl( clearQueryString(fileurl) );

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
      CURLcode ret = curl_easy_setopt( curl, CURLOPT_URL,
                                       urlBuffer.c_str() );
      if ( ret != 0 ) {
        ZYPP_THROW(MediaCurlSetOptException(fileurl, _curlError));
      }

      // Set callback and perform.
      internal::ProgressData progressData( file, curl, settings.timeout(), fileurl, srcFile.downloadSize(), &report );

      ret = curl_easy_setopt( curl, CURLOPT_WRITEDATA,  &progressData  );
      if ( ret != 0 ) {
        ZYPP_THROW(MediaCurlSetOptException(fileurl, _curlError));
      }

      ret = curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, &MediaCurl::writeCallback );
      if ( ret != 0 ) {
        ZYPP_THROW(MediaCurlSetOptException(fileurl, _curlError));
      }

      report->start(fileurl, dest);

      if ( curl_easy_setopt( curl, CURLOPT_PROGRESSDATA, &progressData ) != 0 ) {
        WAR << "Can't set CURLOPT_PROGRESSDATA: " << _curlError << endl;;
      }

      ret = executeCurl( rData );

      // flush buffers
      fflush ( file );

  #if CURLVERSION_AT_LEAST(7,19,4)
      // bnc#692260: If the client sends a request with an If-Modified-Since header
      // with a future date for the server, the server may respond 200 sending a
      // zero size file.
      // curl-7.19.4 introduces CURLINFO_CONDITION_UNMET to check this condition.
      if ( ftell(file) == 0 && ret == 0 )
      {
        long httpReturnCode = 33;
        if ( curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &httpReturnCode ) == CURLE_OK && httpReturnCode == 200 )
        {
          long conditionUnmet = 33;
          if ( curl_easy_getinfo( curl, CURLINFO_CONDITION_UNMET, &conditionUnmet ) == CURLE_OK && conditionUnmet )
          {
            WAR << "TIMECONDITION unmet - retry without." << endl;
            curl_easy_setopt( curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_NONE);
            curl_easy_setopt( curl, CURLOPT_TIMEVALUE, 0L);
            ret = executeCurl( rData );
          }
        }
      }
  #endif

      if ( curl_easy_setopt( curl, CURLOPT_PROGRESSDATA, NULL ) != 0 ) {
        WAR << "Can't unset CURLOPT_PROGRESSDATA: " << _curlError << endl;;
      }

      if ( ret != 0 ) {
        ERR << "curl error: " << ret << ": " << _curlError
            << ", temp file size " << ftell(file)
            << " bytes." << endl;

        // the timeout is determined by the progress data object
        // which holds whether the timeout was reached or not,
        // otherwise it would be a user cancel

        if ( progressData.fileSizeExceeded() )
          ZYPP_THROW(MediaFileSizeExceededException(fileurl, progressData.expectedFileSize()));

        evaluateCurlCode( rData, srcFile.filename(), ret, progressData.timeoutReached() );
      }

      long httpReturnCode = 0;
      CURLcode infoRet = curl_easy_getinfo(curl,
                                           CURLINFO_RESPONSE_CODE,
                                           &httpReturnCode);
      bool modified = true;
      if (infoRet == CURLE_OK)
      {
        DBG << "HTTP response: " + str::numstring(httpReturnCode);
        if ( httpReturnCode == 304
             || ( httpReturnCode == 213 && (endpoint.url().getScheme() == "ftp" || endpoint.url().getScheme() == "tftp") ) ) // not modified
        {
          DBG << " Not modified.";
          modified = false;
        }
        DBG << endl;
      }
      else
      {
        WAR << "Could not get the response code." << endl;
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
      break;  // success!
    }
    catch (MediaUnauthorizedException & ex_r)
    {
      if ( authenticate( endpoint.url(), settings, ex_r.hint(), firstAuth) ) {
        firstAuth = false;  // must not return stored credentials again
        continue; // retry
      }

      report->finish(fileurl, zypp::media::DownloadProgressReport::ACCESS_DENIED, ex_r.asUserHistory());
      ZYPP_RETHROW(ex_r);
    }
    // unexpected exception
    catch (MediaException & excpt_r)
    {
      if ( typeid(excpt_r) == typeid( MediaCurlExceptionMayRetryInternaly ) ) {
        ++internalTry;
        if ( internalTry < maxInternalTry ) {
          // just report (NO_ERROR); no interactive request to the user
          report->problem(fileurl, media::DownloadProgressReport::NO_ERROR, excpt_r.asUserHistory()+_("Will try again..."));
          continue; // retry
        }
        excpt_r.addHistory( str::Format(_("Giving up after %1% attempts.")) % maxInternalTry );
      }

      media::DownloadProgressReport::Error reason = media::DownloadProgressReport::ERROR;
      if( typeid(excpt_r) == typeid( media::MediaFileNotFoundException )  ||
          typeid(excpt_r) == typeid( media::MediaNotAFileException ) )
      {
        reason = media::DownloadProgressReport::NOT_FOUND;
      }
      report->finish(fileurl, reason, excpt_r.asUserHistory());
      ZYPP_RETHROW(excpt_r);
    }
  } while ( true );

  report->finish(fileurl, zypp::media::DownloadProgressReport::NO_ERROR, "");
}

///////////////////////////////////////////////////////////////////

bool MediaCurl::getDoesFileExist( const Pathname & filename ) const
{
  // we need a non const pointer to work around the current API
  auto that = const_cast<MediaCurl *>(this);

  std::exception_ptr lastErr;
  for ( int i : mirrorOrder( OnMediaLocation(filename).setMirrorsAllowed(false) )) {
    try {
      return that->doGetDoesFileExist( i, filename );

    } catch (MediaException & excpt_r) {
      if ( !canTryNextMirror ( excpt_r ) )
        ZYPP_RETHROW(excpt_r);
      lastErr = ZYPP_FWD_CURRENT_EXCPT();
    }
  }
  if ( lastErr ) {
    try {
      ZYPP_RETHROW( lastErr );
    } catch ( const MediaFileNotFoundException &e ) {
      // on file not found we return false
      ZYPP_CAUGHT(e);
      return false;
    }
  }
  return false;
}

///////////////////////////////////////////////////////////////////

void MediaCurl::evaluateCurlCode(RequestData &rData,
                                 const Pathname &filename,
                                 CURLcode code,
                                 bool timeout_reached) const
{
  if ( code != 0 )
  {
    const auto &baseMirr = _origin[rData.mirror];
    Url url;
    if (filename.empty())
      url = baseMirr.url();
    else
      url = getFileUrl(rData.mirror, filename);

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
        CURLcode infoRet = curl_easy_getinfo( rData.curl,
                                              CURLINFO_RESPONSE_CODE,
                                              &httpReturnCode );
        if ( infoRet == CURLE_OK )
        {
          std::string msg = "HTTP response: " + str::numstring( httpReturnCode );
          switch ( httpReturnCode )
          {
          case 401:
          {
              std::string auth_hint = getAuthHint( rData.curl );

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
              ZYPP_THROW(MediaFileNotFoundException(baseMirr.url(), filename));
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
        ZYPP_THROW(MediaFileNotFoundException(baseMirr.url(), filename));
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

bool MediaCurl::doGetDoesFileExist( const int mirror, const Pathname & filename )
{
  DBG << filename.asString() << endl;

  AutoDispose<CURL*> curl( curl_easy_init(), []( CURL *hdl ) { if ( hdl ) { curl_easy_cleanup(hdl); } }  );
  RequestData rData;
  rData.mirror = mirror;
  rData.curl = curl.value ();

  auto &endpoint = _origin[mirror];

  if( !endpoint.url().isValid() )
    ZYPP_THROW(MediaBadUrlException(endpoint.url()));

  if( endpoint.url().getHost().empty() )
    ZYPP_THROW(MediaBadUrlEmptyHostException(endpoint.url()));

  Url url(getFileUrl(mirror, filename));

  DBG << "URL: " << url.asString() << endl;
  // Use URL without options and without username and passwd
  // (some proxies dislike them in the URL).
  // Curl seems to need the just scheme, hostname and a path;
  // the rest was already passed as curl options (in attachTo).
  Url curlUrl( clearQueryString(url) );

  // See also Bug #154197 and ftp url definition in RFC 1738:
  // The url "ftp://user@host/foo/bar/file" contains a path,
  // that is relative to the user's home.
  // The url "ftp://user@host//foo/bar/file" (or also with
  // encoded slash as %2f) "ftp://user@host/%2ffoo/bar/file"
  // contains an absolute path.
  //
  _lastRedirect.clear();
  std::string urlBuffer( curlUrl.asString());

  CURLcode ok;
  bool canRetry  = true;
  bool firstAuth = true;
  auto &settings = endpoint.getConfig<TransferSettings>( MIRR_SETTINGS_KEY.data() );

  while ( canRetry ) {
    canRetry = false;
    setupEasy( rData, settings );

    CURLcode ret = curl_easy_setopt( curl, CURLOPT_URL,
                                     urlBuffer.c_str() );
    if ( ret != 0 ) {
      ZYPP_THROW(MediaCurlSetOptException(url, _curlError));
    }

    AutoFILE file { ::fopen( "/dev/null", "w" ) };
    if ( !file ) {
      ERR << "fopen failed for /dev/null" << endl;
      ZYPP_THROW(MediaWriteException("/dev/null"));
    }

    ret = curl_easy_setopt( curl, CURLOPT_WRITEDATA, (*file) );
    if ( ret != 0 ) {
      ZYPP_THROW(MediaCurlSetOptException(url, _curlError));
    }

    // If no head requests allowed (?head_requests=no):
    // Instead of returning no data with NOBODY, we return
    // little data, that works with broken servers, and
    // works for ftp as well, because retrieving only headers
    // ftp will return always OK code ?
    // See http://curl.haxx.se/docs/knownbugs.html #58
    const bool doHeadRequest = (endpoint.url().getScheme() == "http" || endpoint.url().getScheme() == "https") && settings.headRequestsAllowed();
    if ( doHeadRequest ) {
      curl_easy_setopt( curl, CURLOPT_NOBODY, 1L );
    } else {
      curl_easy_setopt( curl, CURLOPT_RANGE, "0-1" );
    }

    try {
      ok = const_cast<MediaCurl *>(this)->executeCurl( rData );
      MIL << "perform code: " << ok << " [ " << curl_easy_strerror(ok) << " ]" << endl;

      // as we are not having user interaction, the user can't cancel
      // the file existence checking, a callback or timeout return code
      // will be always a timeout.
      evaluateCurlCode( rData, filename, ok, true /* timeout */);
    }
    catch ( const MediaFileNotFoundException &e ) {
      // if the file did not exist then we can return false
      return false;
    }
    catch ( const MediaUnauthorizedException &e ) {
      if ( authenticate( endpoint.url(), settings, e.hint(), firstAuth ) ) {
        firstAuth = false;
        canRetry = true;
        continue;
      }
    }

    // exists
    return ( ok == CURLE_OK );
  }

  return false;
}

///////////////////////////////////////////////////////////////////
//
int MediaCurl::aliveCallback( void *clientp, curl_off_t /*dltotal*/, curl_off_t dlnow, curl_off_t /*ultotal*/, curl_off_t /*ulnow*/ )
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

int MediaCurl::progressCallback( void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow )
{
  internal::ProgressData *pdata = reinterpret_cast<internal::ProgressData *>( clientp );
  if( pdata )
  {
    // work around curl bug that gives us old data
    long httpReturnCode = 0;
    if ( curl_easy_getinfo( pdata->curl(), CURLINFO_RESPONSE_CODE, &httpReturnCode ) != CURLE_OK || httpReturnCode == 0 ) {
      return aliveCallback( clientp, dltotal, dlnow, ultotal, ulnow );
    }
    pdata->updateStats( dltotal, dlnow );
    return pdata->reportProgress();
  }
  return 0;
}

size_t MediaCurl::writeCallback( char *ptr, size_t size, size_t nmemb, void *userdata )
{
  internal::ProgressData *pdata = reinterpret_cast<internal::ProgressData *>( userdata );
  if( pdata ) {
    return pdata->writeBytes ( ptr, size * nmemb );
  }
  return 0;
}

///////////////////////////////////////////////////////////////////

std::string MediaCurl::getAuthHint( CURL *curl ) const
{
  long auth_info = CURLAUTH_NONE;

  CURLcode infoRet =
    curl_easy_getinfo(curl, CURLINFO_HTTPAUTH_AVAIL, &auth_info);

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
    data->expectedFileSize( expectedFileSize );
  }
}

/*!
 * Executes the given curl handle in a multi context
 *
 * \note this should not be a const function, but API forces us to do so
 */
CURLcode MediaCurl::executeCurl( RequestData &rData )
{
  CURL *curl = rData.curl;
  const auto &baseUrl = _origin.at(rData.mirror);

  if (!_multi)
    ZYPP_THROW(MediaCurlInitException(baseUrl.url()));

  internal::CurlPollHelper _curlHelper(*this);

  // add the easy handle to the multi instance
  if ( curl_multi_add_handle( _multi, curl ) != CURLM_OK )
    ZYPP_THROW(MediaCurlException( baseUrl.url(), "curl_multi_add_handle", "unknown error"));

  // make sure the handle is cleanly removed from the multi handle
  OnScopeExit autoRemove([&](){ curl_multi_remove_handle( _multi, curl ); });

  // kickstart curl, this will cause libcurl to go over the added handles and register sockets and timeouts
  CURLMcode mcode = _curlHelper.handleTimout();
  if (mcode != CURLM_OK)
    ZYPP_THROW(MediaCurlException( baseUrl.url(), "curl_multi_socket_action", "unknown error"));

  bool canContinue = true;
  while ( canContinue ) {

    CURLMsg *msg = nullptr;
    int nqueue = 0;
    while ((msg = curl_multi_info_read( _multi, &nqueue)) != 0) {
        if ( msg->msg != CURLMSG_DONE  ) continue;
        if ( msg->easy_handle != curl ) continue;

        return msg->data.result;
    }

    // copy watched sockets in case curl changes the vector as we go over the events later
    std::vector<GPollFD> requestedFds = _curlHelper.socks;

    int r = zypp_detail::zypp_poll( requestedFds, _curlHelper.timeout_ms.value_or( -1 ) );
    if ( r == -1 )
      ZYPP_THROW( MediaCurlException(baseUrl.url(), "zypp_poll() failed", "unknown error") );

    // run curl
    if ( r == 0 ) {
      CURLMcode mcode = _curlHelper.handleTimout();
      if (mcode != CURLM_OK)
        ZYPP_THROW(MediaCurlException(baseUrl.url(), "curl_multi_socket_action", "unknown error"));
    } else {
      CURLMcode mcode = _curlHelper.handleSocketActions( requestedFds );
      if (mcode != CURLM_OK)
        ZYPP_THROW(MediaCurlException(baseUrl.url(), "curl_multi_socket_action", "unknown error"));
    }
  }
  return CURLE_OK;
}


  } // namespace media
} // namespace zypp
//
