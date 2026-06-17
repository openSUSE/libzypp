/*---------------------------------------------------------------------\
|                          ____ _   __ __ ___                          |
|                         |__  / \ / / . \ . \                         |
|                           / / \ V /|  _/  _/                         |
|                          / /__ | | | | | |                           |
|                         /_____||_| |_| |_|                           |
|                                                                      |
----------------------------------------------------------------------/
*/
#ifndef ZYPP_CURL_NG_NETWORK_HTTPHEADER_H_INCLUDED
#define ZYPP_CURL_NG_NETWORK_HTTPHEADER_H_INCLUDED

#include <string>
#include <zypp-core/base/String.h>

namespace zyppng
{
  /** Store and provide HTTP header data (key/value).
   * Use \ref asString to get a valid HTTP header.
   */
  class HttpHeader
  {
  public:
    HttpHeader( std::string key_r, std::string value_r )
    : _key   { zypp::str::trim( key_r ) }
    , _value { zypp::str::trim( value_r ) }
    {}

    const std::string & key()   const { return _key; }
    const std::string & value() const { return _value; }

    /** Return valid HTTP header string.
     * bsc#1212187: HTTP/2 RFC 9113 forbids fields ending with a space.
     * Empty field values - not desired but legal - however need to be
     * encoded as 'key: '. The space after the colon is not part of the
     * header's value; it is a structural separator required by HTTP/1.1.
     * For HTTP/2 it's not part of the field value.
     * */
    std::string asString() const
    { return _key + ": " + _value; }

    static std::string asString( std::string key_r, std::string value_r )
    { return HttpHeader( key_r, value_r ).asString(); }

  private:
    std::string _key;
    std::string _value;
  };
} // namespace zyppng
#endif // ZYPP_CURL_NG_NETWORK_HTTPHEADER_H_INCLUDED
