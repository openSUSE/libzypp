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

#include <zypp-core/Globals.h>
#include <zypp-core/AutoDispose.h>

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
#include <fstream>
#include <unistd.h>
#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

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

  long maxlvl = *(static_cast<long*>(max_lvl));
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


void prepareSettingsAndUrl( zypp::Url &url_r, zypp::media::TransferSettings &s )
{
  ::internal::fillSettingsFromUrl ( url_r, s );

  // if the proxy was not set (or explicitly unset) by url, then look...
  if ( s.proxy().empty() )
    ::internal::fillSettingsSystemProxy( url_r, s );

  // remove extra options from the URL
  url_r = ::internal::clearQueryString( url_r );
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
      if ( ! param.empty() ) {
        pDBG( "setPassword from", url );
        s.setPassword( std::move(param) );
      }
    }
    else
    {
      // if there is no username, set anonymous auth
      if ( url.schemeIsFtpLike() )
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
    // HTTP/FTP authentication type
    if ( url.schemeIsHttpLike() ) {
      std::string param { url.getQueryParam("auth") };
      if ( ! param.empty() )
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
    } else if ( url.schemeIsFtpLike() ) {
      // Well, there's is not much else, but setting it explicitly in advance
      // will also preload the credentials before the 1st request.
      s.setAuthType( "basic" );
    }
  }
  {
    // workarounds
    const std::string & param { url.getQueryParam("head_requests") };
    if( ! param.empty() && param == "no" )
      s.setHeadRequestsAllowed( false );
  }
  {
    const auto &cookieFileParam = url.getQueryParam( "cookies" );
    if ( !cookieFileParam.empty() && str::strToBool( cookieFileParam, true ) )
      s.setEnableCookieFile ( true );
    else
      s.setEnableCookieFile ( false );
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

std::string curlUnEscape( const std::string& text_r ) {
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
  curlUrl.delQueryParams( {
    "cookies",
    "proxy",
    "proxyport",
    "proxyuser",
    "proxypass",
    "ssl_capath",
    "ssl_verify",
    "ssl_clientcert",
    "ssl_clientkey",
    "timeout",
    "auth",
    "username",
    "password",
    "mediahandler",
    "credentials",
    "head_requests",
    "mediahandler",
  } );
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
  for ( size_t sock = first; sock < actionsFds.size(); sock++ ) {
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
CURLcode setCurlRedirProtocols(CURL *curl)
{
#if CURLVERSION_AT_LEAST(7,19,4)
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
#endif // #if CURLVERSION_AT_LEAST(7,19,4)
  return CURLE_OK;
}

namespace {

  std::string x509NameToString( X509_NAME *name_r )
  {
    std::string ret;
    if ( !name_r )
      return ret;
    zypp::AutoDispose<BIO *> bio( BIO_new( BIO_s_mem() ), BIO_free );
    if ( !bio )
      return ret;
    if ( X509_NAME_print_ex( bio, name_r, 0, XN_FLAG_RFC2253 ) >= 0 ) {
      char *data = nullptr;
      long len = BIO_get_mem_data( bio.value(), &data );
      if ( data && len > 0 )
        ret.assign( data, len );
    }
    return ret;
  }

  /** Frees the SslVerifyInfoPtr copy owned by an SSL_CTX. */
  void sslVerifyInfoFree( void * /*parent*/, void *ptr, CRYPTO_EX_DATA * /*ad*/, int /*idx*/, long /*argl*/, void * /*argp*/ )
  {
    delete static_cast<SslVerifyInfoPtr *>( ptr );
  }

  int sslCtxExIndex()
  {
    static const int idx = SSL_CTX_get_ex_new_index( 0, nullptr, nullptr, nullptr, &sslVerifyInfoFree );
    return idx;
  }

  /** Only touch curl's SSL_CTX if curl uses the same libssl as we do. */
  bool curlUsesOurOpenSSL()
  {
    static const bool ret = [](){
      const curl_version_info_data *vi = curl_version_info( CURLVERSION_NOW );
      const std::string have( ( vi && vi->ssl_version ) ? vi->ssl_version : "" );
      const unsigned long v = OpenSSL_version_num();
      const std::string want( "OpenSSL/" + std::to_string( (v >> 28) & 0xF ) + "." + std::to_string( (v >> 20) & 0xFF ) + "." );
      const bool ok = zypp::str::startsWith( have, want );
      if ( ok )
        MIL << "TLS verify details enabled (curl: " << have << ")" << endl;
      else
        WAR << "TLS verify details disabled: curl TLS backend '" << have << "' does not match '" << want << "'" << endl;
      return ok;
    }();
    return ret;
  }

  int sslVerifyCallback( int preverifyOk, X509_STORE_CTX *storeCtx )
  {
    SslVerifyInfoPtr *holder = nullptr;
    if ( SSL *ssl = static_cast<SSL *>( X509_STORE_CTX_get_ex_data( storeCtx, SSL_get_ex_data_X509_STORE_CTX_idx() ) ) )
      holder = static_cast<SslVerifyInfoPtr *>( SSL_CTX_get_ex_data( SSL_get_SSL_CTX( ssl ), sslCtxExIndex() ) );
    if ( !holder || !*holder )
      return preverifyOk;

    SslVerifyInfo &info = **holder;
    if ( !info.haveLeaf ) {
      if ( X509 *leaf = X509_STORE_CTX_get0_cert( storeCtx ) ) {
        info.leafSubject = x509NameToString( X509_get_subject_name( leaf ) );
        info.leafIssuer  = x509NameToString( X509_get_issuer_name( leaf ) );
        info.haveLeaf = true;
      }
    }
    if ( !preverifyOk && info.x509Error == 0 ) {
      info.x509Error      = X509_STORE_CTX_get_error( storeCtx );
      info.x509ErrorDepth = X509_STORE_CTX_get_error_depth( storeCtx );
    }
    // Never change the verification result.
    return info.prevVerifyCb ? info.prevVerifyCb( preverifyOk, storeCtx ) : preverifyOk;
  }

  CURLcode sslCtxCallback( CURL * /*curl*/, void *sslctx, void *userptr )
  {
    auto *owner = static_cast<SslVerifyInfoPtr *>( userptr );
    const int idx = sslCtxExIndex();
    if ( !owner || !*owner || !sslctx || idx < 0 )
      return CURLE_OK;

    SSL_CTX *ctx = static_cast<SSL_CTX *>( sslctx );
    SslVerifyInfo &info = **owner;
    info.reset();   // one handshake = one record; the last handshake (e.g. after a redirect) wins
    info.prevVerifyCb = SSL_CTX_get_verify_callback( ctx );
    if ( info.prevVerifyCb == &sslVerifyCallback )  // same SSL_CTX seen again: never chain to ourselves
      info.prevVerifyCb = nullptr;

    // The SSL_CTX owns its own shared_ptr copy, it may outlive the request in curl's connection cache.
    if ( auto *existing = static_cast<SslVerifyInfoPtr *>( SSL_CTX_get_ex_data( ctx, idx ) ) ) {
      *existing = *owner;
    } else {
      auto holder = std::make_unique<SslVerifyInfoPtr>( *owner );
      if ( SSL_CTX_set_ex_data( ctx, idx, holder.get() ) != 1 )
        return CURLE_OK;
      holder.release();   // now owned by the SSL_CTX, freed by sslVerifyInfoFree
    }
    SSL_CTX_set_verify( ctx, SSL_CTX_get_verify_mode( ctx ), &sslVerifyCallback );
    return CURLE_OK;
  }

  std::string readFirstLine( const std::string &path_r )
  {
    std::ifstream in( path_r );
    std::string line;
    std::getline( in, line );
    return line;
  }

  std::string callerInfo()
  {
    const pid_t ppid = ::getppid();
    const char *sudoUser = ::getenv( "SUDO_USER" );
    return zypp::str::Format( "pid=%1% ppid=%2% ppid_comm='%3%' uid=%4% euid=%5% loginuid=%6% sudo_user='%7%'" )
           % ::getpid() % ppid % readFirstLine( "/proc/" + zypp::str::numstring( ppid ) + "/comm" )
           % ::getuid() % ::geteuid() % readFirstLine( "/proc/self/loginuid" ) % ( sudoUser ? sudoUser : "" );
  }

} // namespace

void setupSslVerifyInfo( CURL *curl, SslVerifyInfoPtr &info_r )
{
  if ( !info_r )
    info_r = std::make_shared<SslVerifyInfo>();
  info_r->reset();

  if ( !curl || !curlUsesOurOpenSSL() || sslCtxExIndex() < 0 )
    return;

  if ( curl_easy_setopt( curl, CURLOPT_SSL_CTX_FUNCTION, &sslCtxCallback ) != CURLE_OK
       || curl_easy_setopt( curl, CURLOPT_SSL_CTX_DATA, &info_r ) != CURLE_OK ) {
    curl_easy_setopt( curl, CURLOPT_SSL_CTX_FUNCTION, static_cast<curl_ssl_ctx_callback>( nullptr ) );
    static bool once __attribute__ ((__unused__)) = ( [] {
      WAR << "TLS verify details disabled: curl does not support CURLOPT_SSL_CTX_FUNCTION" << endl;
    } (), true );
  }
}

bool isSslVerifyFailure( CURLcode code )
{
#if CURLVERSION_AT_LEAST(7,62,0)
  return code == CURLE_PEER_FAILED_VERIFICATION; // CURLE_SSL_CACERT is an alias since 7.62.0
#else
  return code == CURLE_PEER_FAILED_VERIFICATION || code == CURLE_SSL_CACERT;
#endif
}

std::string sslVerifyFailedMessage( CURL *curl, const std::string &curlError_r, const SslVerifyInfo &info_r )
{
  std::string url;
  char *effUrl = nullptr;
  if ( curl && curl_easy_getinfo( curl, CURLINFO_EFFECTIVE_URL, &effUrl ) == CURLE_OK && effUrl ) {
    try {
      url = zypp::Url( effUrl ).asString(); // hides a password
    } catch ( ... ) {
      url = "<unparsable url>";
    }
  }
  long verifyResult = -1;
  if ( curl )
    curl_easy_getinfo( curl, CURLINFO_SSL_VERIFYRESULT, &verifyResult );

  std::string details;
  std::string x509Err;
  if ( info_r.x509Error != 0 ) {
    const char *errStr = X509_verify_cert_error_string( info_r.x509Error );
    x509Err = zypp::str::Format( "%1% (%2%) at depth %3%" ) % ( errStr ? errStr : "unknown" ) % info_r.x509Error % info_r.x509ErrorDepth;
  }
  if ( info_r.haveLeaf ) {
    details = "Server certificate subject: " + info_r.leafSubject
            + "\nServer certificate issuer: " + info_r.leafIssuer;
    if ( !x509Err.empty() )
      details += "\nCertificate verify error: " + x509Err;
  }

  ERR << "TLS certificate verification failed: url='" << url << "'"
      << " curl_error='" << curlError_r << "'"
      << " ssl_verify_result=" << verifyResult
      << " subject='" << ( info_r.haveLeaf ? info_r.leafSubject : std::string("<unknown>") ) << "'"
      << " issuer='"  << ( info_r.haveLeaf ? info_r.leafIssuer  : std::string("<unknown>") ) << "'"
      << " x509_error='" << x509Err << "'"
      << " caller: " << callerInfo() << endl;

  return details.empty() ? curlError_r : curlError_r + "\n" + details;
}

}
