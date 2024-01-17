/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
\---------------------------------------------------------------------*/
/** \file zypp-curl/curlhelper.cc
 *
*/
#include "private/curlhelper_p.h"

#include <zypp/APIConfig.h>

#include <zypp-core/fs/PathInfo.h>
#include <zypp-core/Pathname.h>
#include <zypp-core/base/LogTools.h>
#include <zypp-core/base/String.h>
#include <zypp-core/base/StringV.h>
#include <zypp-curl/ProxyInfo>
#include <zypp-curl/auth/CurlAuthData>
#include <zypp-media/MediaException>
#include <string>
#include <glib.h>

#define  TRANSFER_TIMEOUT_MAX   60 * 60

using std::endl;
using namespace zypp;

namespace zypp
{
  namespace env
  {
    const long & ZYPP_MEDIA_CURL_DEBUG()
    {
      static const long ret = [](){
        const char * env = getenv("ZYPP_MEDIA_CURL_DEBUG");
        return env && *env ? str::strtonum<ulong>( env ) : 0;
      }();
      return ret;
    }

    int ZYPP_MEDIA_CURL_IPRESOLVE()
    {
      static int _v = [](){
        int ret = 0;
        if ( const char * envp = getenv( "ZYPP_MEDIA_CURL_IPRESOLVE" ) ) {
          WAR << "env set: $ZYPP_MEDIA_CURL_IPRESOLVE='" << envp << "'" << std::endl;
          if (      strcmp( envp, "4" ) == 0 )	ret = 4;
          else if ( strcmp( envp, "6" ) == 0 )	ret = 6;
        }
        return ret;
      }();
      return _v;
    }
  } // namespace env
} // namespace zypp

namespace internal
{

void globalInitCurlOnce()
{
  // function-level static <=> std::call_once
  static bool once __attribute__ ((__unused__)) = ( [] {
    MIL << "global_init libcurl: build version: (" << LIBCURL_VERSION << "), runtime version: (" << curl_version_info(CURLVERSION_NOW)->version << ") " << endl;
    if ( curl_global_init( CURL_GLOBAL_ALL ) != 0 )
      WAR << "curl global init failed" << std::endl;
  } (), true );
}

uint curlVersion()
{
  auto curlV = curl_version_info ( CURLVERSION_NOW );
  return curlV->version_num;
}

int log_curl( CURL * curl, curl_infotype info, char * ptr, size_t len, void * max_lvl )
{
  if ( max_lvl == nullptr )
    return 0;

  long maxlvl = *((long *)max_lvl);
  const char * pfx = "";
  bool isContent = true;  // otherwise it's data
  switch( info )
  {
    case CURLINFO_TEXT:         if ( maxlvl < 1 ) return 0; pfx = "*"; break;
    case CURLINFO_HEADER_IN:    if ( maxlvl < 2 ) return 0; pfx = "<"; break;
    case CURLINFO_HEADER_OUT:   if ( maxlvl < 2 ) return 0; pfx = ">"; break;
    case CURLINFO_SSL_DATA_IN:  if ( maxlvl < 3 ) return 0; isContent = false; pfx = "<[SSL]"; break;
    case CURLINFO_SSL_DATA_OUT: if ( maxlvl < 3 ) return 0; isContent = false; pfx = ">[SSL]"; break;
    case CURLINFO_DATA_IN:      if ( maxlvl < 3 ) return 0; isContent = false; pfx = "<[DTA]"; break;
    case CURLINFO_DATA_OUT:     if ( maxlvl < 3 ) return 0; isContent = false; pfx = ">[DTA]"; break;

    default:
      return 0;
  }

  // We'd like to keep all log messages within function `log_curl`
  // because this tag to grep for is known and communicate to users.
  if ( isContent ) {
    std::vector<std::string_view> lines;  // don't want log from within the lambda
    strv::split( std::string_view( ptr, len ), "\n", [&lines]( std::string_view line, unsigned, bool last ) {
      if ( last ) return; // empty word after final \n
      line = strv::rtrim( line, "\r" );
      lines.push_back( line );
    });
    for ( const auto & line : lines ) {
      if ( str::hasPrefix( line, "Authorization:" ) ) {
        std::string_view::size_type pos { line.find( " ", 15 ) }; // Authorization: <type> <credentials>
        if ( pos == std::string::npos )
          pos = 15;
        DBG << curl << " " << pfx << " " << line.substr( 0, pos ) << " <credentials removed>" << endl;
      }
      else
        DBG << curl << " " << pfx << " " << line << endl;
    }
  } else {
    if ( maxlvl < 4 )
      DBG << curl << " " << pfx << " " << len << " byte" << endl;
    else
      hexdumpOn( DBG << curl << " " << pfx << " ", ptr, len );
  }
  return 0;
}

void setupZYPP_MEDIA_CURL_DEBUG( CURL *curl )
{
  if ( not curl ) {
    INT << "Got a NULL curl handle" << endl;
    return;
  }
  if ( env::ZYPP_MEDIA_CURL_DEBUG() > 0 ) {
    curl_easy_setopt( curl, CURLOPT_VERBOSE, 1L );
    curl_easy_setopt( curl, CURLOPT_DEBUGFUNCTION, log_curl );
    curl_easy_setopt( curl, CURLOPT_DEBUGDATA, &env::ZYPP_MEDIA_CURL_DEBUG() );
  }
}

size_t log_redirects_curl( char *ptr, size_t size, size_t nmemb, void *userdata)
{
  //INT << "got header: " << std::string(ptr, ptr + size*nmemb) << endl;

  char * lstart = ptr, * lend = ptr;
  size_t pos = 0;
  size_t max = size * nmemb;
  while (pos + 1 < max)
  {
    // get line
    for (lstart = lend; *lend != '\n' && pos < max; ++lend, ++pos);

    // look for "Location"
    if ( strncasecmp( lstart, "Location:", 9 ) == 0 )
    {
      std::string line { lstart, *(lend-1)=='\r' ? lend-1 : lend };
      DBG << "redirecting to " << line << std::endl;
      if ( userdata ) {
        *reinterpret_cast<std::string *>( userdata ) = line;
      }
      return max;
    }

    // continue with the next line
    if (pos + 1 < max)
    {
      ++lend;
      ++pos;
    }
    else
      break;
  }

  return max;
}

/**
 * Fills the settings structure using options passed on the url
 * for example ?timeout=x&proxy=foo
 */
void fillSettingsFromUrl( const Url &url, media::TransferSettings &s )
{
  {
    const std::string & param { url.getQueryParam("timeout") };
    if( ! param.empty() )
    {
      long num = str::strtonum<long>(param);
      if( num >= 0 && num <= TRANSFER_TIMEOUT_MAX )
        s.setTimeout( num );
    }
  }
  {
    std::string param { url.getUsername() };
    if ( ! param.empty() )
    {
      s.setUsername( std::move(param) );
      param = url.getPassword();
      if ( ! param.empty() )
        s.setPassword( std::move(param) );
    }
    else
    {
      // if there is no username, set anonymous auth
      if ( ( url.getScheme() == "ftp" || url.getScheme() == "tftp" ) && s.username().empty() )
        s.setAnonymousAuth();
    }
  }
  if ( url.getScheme() == "https" )
  {
    s.setVerifyPeerEnabled( false );
    s.setVerifyHostEnabled( false );

    const std::string & verify { url.getQueryParam("ssl_verify") };
    if( verify.empty() || verify == "yes" )
    {
      s.setVerifyPeerEnabled( true );
      s.setVerifyHostEnabled( true );
    }
    else if ( verify == "no" )
    {
      s.setVerifyPeerEnabled( false );
      s.setVerifyHostEnabled( false );
    }
    else
    {
      std::vector<std::string> flags;
      str::split( verify, std::back_inserter(flags), "," );
      for ( const auto & flag : flags )
      {
        if ( flag == "host" )
          s.setVerifyHostEnabled( true );
        else if ( flag == "peer" )
          s.setVerifyPeerEnabled( true );
        else
          ZYPP_THROW( media::MediaBadUrlException(url, "Unknown ssl_verify flag "+flag) );
      }
    }
  }
  {
    Pathname ca_path { url.getQueryParam("ssl_capath") };
    if( ! ca_path.empty() )
    {
      if( ! PathInfo(ca_path).isDir() || ! ca_path.absolute() )
        ZYPP_THROW(media::MediaBadUrlException(url, "Invalid ssl_capath path"));
      else
        s.setCertificateAuthoritiesPath( std::move(ca_path) );
    }
  }
  {
    Pathname client_cert { url.getQueryParam("ssl_clientcert") };
    if( ! client_cert.empty() )
    {
      if( ! PathInfo(client_cert).isFile() || ! client_cert.absolute() )
        ZYPP_THROW(media::MediaBadUrlException(url, "Invalid ssl_clientcert file"));
      else
        s.setClientCertificatePath( std::move(client_cert) );
    }
  }
  {
    Pathname client_key { url.getQueryParam("ssl_clientkey") };
    if( ! client_key.empty() )
    {
      if( ! PathInfo(client_key).isFile() || ! client_key.absolute() )
        ZYPP_THROW(media::MediaBadUrlException(url, "Invalid ssl_clientkey file"));
      else
        s.setClientKeyPath( std::move(client_key) );
    }
  }
  {
    std::string param { url.getQueryParam( "proxy" ) };
    if ( ! param.empty() )
    {
      if ( param == EXPLICITLY_NO_PROXY ) {
        // Workaround TransferSettings shortcoming: With an
        // empty proxy string, code will continue to look for
        // valid proxy settings. So set proxy to some non-empty
        // string, to indicate it has been explicitly disabled.
        s.setProxy(EXPLICITLY_NO_PROXY);
        s.setProxyEnabled(false);
      }
      else {
        const std::string & proxyport { url.getQueryParam( "proxyport" ) };
        if ( ! proxyport.empty() ) {
          param += ":";
          param += proxyport;
        }
        s.setProxy( std::move(param) );
        s.setProxyEnabled( true );
      }
    }
  }
  {
    std::string param { url.getQueryParam( "proxyuser" ) };
    if ( ! param.empty() )
    {
      s.setProxyUsername( std::move(param) );
      s.setProxyPassword( url.getQueryParam( "proxypass" ) );
    }
  }
  {
    // HTTP authentication type
    std::string param { url.getQueryParam("auth") };
    if ( ! param.empty() && (url.getScheme() == "http" || url.getScheme() == "https") )
    {
      try
      {
        media::CurlAuthData::auth_type_str2long (param );	// check if we know it
      }
      catch ( const media::MediaException & ex_r )
      {
        DBG << "Rethrowing as MediaUnauthorizedException.";
        ZYPP_THROW(media::MediaUnauthorizedException(url, ex_r.msg(), "", ""));
      }
      s.setAuthType( std::move(param) );
    }
  }
  {
    // workarounds
    const std::string & param { url.getQueryParam("head_requests") };
    if( ! param.empty() && param == "no" )
      s.setHeadRequestsAllowed( false );
  }
}

/**
 * Reads the system proxy configuration and fills the settings
 * structure proxy information
 */
void fillSettingsSystemProxy( const Url& url, media::TransferSettings &s )
{
  media::ProxyInfo proxy_info;
  if ( proxy_info.useProxyFor( url ) )
  {
    // We must extract any 'user:pass' from the proxy url
    // otherwise they won't make it into curl (.curlrc wins).
    try {
      Url u( proxy_info.proxy( url ) );
      s.setProxy( u.asString( url::ViewOption::WITH_SCHEME + url::ViewOption::WITH_HOST + url::ViewOption::WITH_PORT ) );
      // don't overwrite explicit auth settings
      if ( s.proxyUsername().empty() )
      {
        s.setProxyUsername( u.getUsername( url::E_ENCODED ) );
        s.setProxyPassword( u.getPassword( url::E_ENCODED ) );
      }
      s.setProxyEnabled( true );
    }
    catch (...) {}	// no proxy if URL is malformed
  }
}

void curlEscape( std::string & str_r,
  const char char_r, const std::string & escaped_r ) {
  for ( std::string::size_type pos = str_r.find( char_r );
        pos != std::string::npos; pos = str_r.find( char_r, pos ) ) {
    str_r.replace( pos, 1, escaped_r );
  }
}

std::string curlEscapedPath( std::string path_r ) {
  curlEscape( path_r, ' ', "%20" );
  return path_r;
}

std::string curlUnEscape( std::string text_r ) {
  char * tmp = curl_unescape( text_r.c_str(), 0 );
  std::string ret( tmp );
  curl_free( tmp );
  return ret;
}

Url clearQueryString(const Url &url)
{
  Url curlUrl (url);
  curlUrl.setUsername( "" );
  curlUrl.setPassword( "" );
  curlUrl.setPathParams( "" );
  curlUrl.setFragment( "" );
  curlUrl.delQueryParam("cookies");
  curlUrl.delQueryParam("proxy");
  curlUrl.delQueryParam("proxyport");
  curlUrl.delQueryParam("proxyuser");
  curlUrl.delQueryParam("proxypass");
  curlUrl.delQueryParam("ssl_capath");
  curlUrl.delQueryParam("ssl_verify");
  curlUrl.delQueryParam("ssl_clientcert");
  curlUrl.delQueryParam("timeout");
  curlUrl.delQueryParam("auth");
  curlUrl.delQueryParam("username");
  curlUrl.delQueryParam("password");
  curlUrl.delQueryParam("mediahandler");
  curlUrl.delQueryParam("credentials");
  curlUrl.delQueryParam("head_requests");
  return curlUrl;
}

// bsc#933839: propagate proxy settings passed in the repo URL
// boo#1127591: propagate ssl settings passed in the repo URL
zypp::Url propagateQueryParams( zypp::Url url_r, const zypp::Url & template_r )
{
  using namespace std::literals::string_literals;
  for ( const std::string &param : { "proxy"s, "proxyport"s, "proxyuser"s, "proxypass"s, "ssl_capath"s, "ssl_verify"s } )
  {
    const std::string & value( template_r.getQueryParam( param ) );
    if ( ! value.empty() )
      url_r.setQueryParam( param, value );
  }
  return url_r;
}

CurlPollHelper::CurlPollHelper(CurlPoll &p) : _parent(p) {
  curl_multi_setopt( _parent._multi, CURLMOPT_SOCKETFUNCTION, socketcb );
  curl_multi_setopt( _parent._multi, CURLMOPT_SOCKETDATA, this );
  curl_multi_setopt( _parent._multi, CURLMOPT_TIMERFUNCTION, timercb );
  curl_multi_setopt( _parent._multi, CURLMOPT_TIMERDATA, this );
}

CurlPollHelper::~CurlPollHelper() {
  curl_multi_setopt( _parent._multi, CURLMOPT_SOCKETFUNCTION, nullptr );
  curl_multi_setopt( _parent._multi, CURLMOPT_SOCKETDATA, nullptr );
  curl_multi_setopt( _parent._multi, CURLMOPT_TIMERFUNCTION, nullptr );
  curl_multi_setopt( _parent._multi, CURLMOPT_TIMERDATA, nullptr );
}

int CurlPollHelper::socketcb(CURL *easy, curl_socket_t s, int what, CurlPollHelper *userp, void *sockp) {
  auto it = std::find_if( userp->socks.begin(), userp->socks.end(), [&]( const GPollFD &fd){ return fd.fd == s; });
  gushort events = 0;
  if ( what == CURL_POLL_REMOVE ) {
    if ( it == userp->socks.end() ) {
      WAR << "Ignoring unknown socket in static_socketcb" << std::endl;
      return 0;
    }
    userp->socks.erase(it);
    return 0;
  } else if ( what == CURL_POLL_IN ) {
    events = G_IO_IN | G_IO_HUP | G_IO_ERR;
  } else if ( what == CURL_POLL_OUT ) {
    events = G_IO_OUT | G_IO_ERR;
  } else if ( what == CURL_POLL_INOUT ) {
    events = G_IO_IN | G_IO_OUT | G_IO_HUP | G_IO_ERR;
  }

  if ( it != userp->socks.end() ) {
    it->events = events;
    it->revents = 0;
  } else {
    userp->socks.push_back(
          GPollFD{
            .fd = s,
            .events = events,
            .revents = 0
          }
          );
  }
  return 0;
}

int CurlPollHelper::timercb(CURLM *, long timeout_ms, CurlPollHelper *thatPtr) {
  if ( !thatPtr )
    return 0;
  if ( timeout_ms == -1 )
    thatPtr->timeout_ms.reset(); // curl wants to delete its timer
  else
    thatPtr->timeout_ms = timeout_ms; // maximum time curl wants us to sleep
  return 0;
}

CURLMcode internal::CurlPollHelper::handleSocketActions( const std::vector<GPollFD> &actionsFds , int first )
{
  for ( int sock = first; sock < actionsFds.size(); sock++ ) {
    const auto &waitFd = actionsFds[sock];
    if ( waitFd.revents == 0 )
      continue;

    int ev = 0;
    if ( (waitFd.revents & G_IO_HUP) == G_IO_HUP
        || (waitFd.revents & G_IO_IN) == G_IO_IN ) {
      ev = CURL_CSELECT_IN;
    }
    if ( (waitFd.revents & G_IO_OUT) == G_IO_OUT ) {
      ev |= CURL_CSELECT_OUT;
    }
    if ( (waitFd.revents & G_IO_ERR) == G_IO_ERR ) {
      ev |= CURL_CSELECT_ERR;
    }

    int runn = 0;
    CURLMcode mcode = curl_multi_socket_action( _parent._multi, waitFd.fd, ev, &runn );
    if (mcode != CURLM_OK)
      return mcode;
  }
  return CURLM_OK;
}

CURLMcode internal::CurlPollHelper::handleTimout()
{
  int handles = 0;
  return curl_multi_socket_action( _parent._multi, CURL_SOCKET_TIMEOUT, 0, &handles );
}

/*!
 * Enables the redirection protocols that we support, this code is a bit ugly
 * because for newer versions we need to detect the current curl version at runtime, but at the same
 * time need the compile time switch in case we are built against a older libcurl version.
 *
 * Reason for this is that we do not explicitely require the libcurl version we are built against,
 * this could mean we are ending up with a libcurl older than the one we built against.
 *
 * See bsc#1218831
 */
CURLcode setCurlRedirProtocols(CURL *curl, bool enableHttp)
{
#if CURLVERSION_AT_LEAST(7,19,4)
  if ( enableHttp ) {
#if CURLVERSION_AT_LEAST(7,85,0)
    // runtime version might be different from build version
    if( ::internal::curlVersion() >= CURL_VERSION_BITS(7,85,0) ) {
      return curl_easy_setopt ( curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https" );
    } else {
      return curl_easy_setopt ( curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS );
    }
#else
    return curl_easy_setopt ( curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS );
#endif
  } else {
#if CURLVERSION_AT_LEAST(7,85,0)
    // runtime version might be different from build version
    if( ::internal::curlVersion() >= CURL_VERSION_BITS(7,85,0) ) {
      return curl_easy_setopt ( curl, CURLOPT_REDIR_PROTOCOLS_STR, "https" );
    } else {
      return curl_easy_setopt ( curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS );
    }
#else
    return curl_easy_setopt ( curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS );
#endif
  }
#endif // #if CURLVERSION_AT_LEAST(7,19,4)
  return CURLE_OK;
}

}
