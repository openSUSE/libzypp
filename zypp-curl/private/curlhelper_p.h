/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*
* This file contains private API, this might break at any time between releases.
* You have been warned!
*
*/
#ifndef ZYPP_MEDIA_PRIVATE_CURLHELPER_P_H_INCLUDED
#define ZYPP_MEDIA_PRIVATE_CURLHELPER_P_H_INCLUDED

#include <curl/curl.h>
#include <zypp-core/Url.h>
#include <zypp-curl/TransferSettings>

#include <optional>

#define EXPLICITLY_NO_PROXY "_none_"

#undef CURLVERSION_AT_LEAST
#define CURLVERSION_AT_LEAST(M,N,O) LIBCURL_VERSION_NUM >= ((((M)<<8)+(N))<<8)+(O)

typedef struct _GPollFD GPollFD;

namespace zypp
{
  namespace env
  {
    /** \c const long& for setting CURLOPT_DEBUGDATA
     * Returns a reference to a static variable, so it's
     * safe to pass it's address to CURLOPT_DEBUGDATA.
     */
    const long & ZYPP_MEDIA_CURL_DEBUG();

    /** 4/6 to force IPv4/v6 */
    int ZYPP_MEDIA_CURL_IPRESOLVE();
  } // namespace env
} //namespace zypp

//do not export
namespace internal {

void globalInitCurlOnce();
/** Setup CURLOPT_VERBOSE and CURLOPT_DEBUGFUNCTION according to env::ZYPP_MEDIA_CURL_DEBUG. */
void setupZYPP_MEDIA_CURL_DEBUG( CURL *curl );
size_t log_redirects_curl( char *ptr, size_t size, size_t nmemb, void *userdata);


void fillSettingsFromUrl( const zypp::Url &url, zypp::media::TransferSettings &s );
void fillSettingsSystemProxy( const zypp::Url& url, zypp::media::TransferSettings &s );

void curlEscape( std::string & str_r,  const char char_r, const std::string & escaped_r );
std::string curlEscapedPath( std::string path_r );
std::string curlUnEscape( std::string text_r );

zypp::Url clearQueryString(const zypp::Url &url);
zypp::Url propagateQueryParams( zypp::Url url_r, const zypp::Url & template_r );


/*!
 * Helper class to simplify using the curl multi API, takes
 * care of remembering the registered sockets and the required curl timeout.
 */
struct CurlPollHelper
{
  struct CurlPoll {
    CURLM *_multi = nullptr;
  };

  CurlPollHelper( CurlPoll &p );
  ~CurlPollHelper();

  /*!
   * Iterator over the passed in poll fd's and call curl_multi_socket_action on them if one of
   * them signals that events have happened
   */
  CURLMcode handleSocketActions( const std::vector<GPollFD> &actionsFds, int first = 0 );

  /*!
   * Tells libcurl that the requested timeout was reached.
   */
  CURLMcode handleTimout ();

  /*!
   * Callback for libcurl when it wants us to track or stop us from tracking a socket
   */
  static int socketcb (CURL * easy, curl_socket_t s, int what, CurlPollHelper *userp, void *sockp );

  /*!
   * Callback for libcurl when it wants us to start/update/remove a timer
   */
  static int timercb( CURLM *, long timeout_ms, CurlPollHelper *thatPtr );

  CurlPoll &_parent;
  std::vector<GPollFD> socks; //< This is the list of fd's we need to track, events have been set by curl
  std::optional<long> timeout_ms = 0; //if set curl wants a timeout
};

}

#endif //ZYPP_MEDIA_PRIVATE_CURLHELPER_P_H_INCLUDED
